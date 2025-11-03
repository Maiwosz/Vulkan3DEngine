#include "Material.h"
#include "DescriptorSetBuilder.h"
#include <stdexcept>
#include <spdlog/spdlog.h>

Material::Material(
    const std::string& name,
    SmartAssetHandle<ShaderHandle, ShaderAsset> smartShader,
    const std::vector<Parameter>& params,
    const LogicalDevice& device,
    BufferManager& bufferManager,
    ImageSamplerManager& samplerManager,
    TextureManager& textureManager,
    DescriptorAllocator& descriptorAllocator,
    DescriptorLayoutManager& descriptorLayoutManager
)
    : m_name(name),
    m_shader(smartShader),
    m_parameters(params),
    m_device(device),
    m_bufferManager(bufferManager),
    m_samplerManager(samplerManager),
    m_textureManager(textureManager),
    m_descriptorAllocator(descriptorAllocator),
    m_descriptorLayoutManager(descriptorLayoutManager),
    m_descriptorSetValid(false)
{
    // Build parameter indices map for quick lookup
    for (size_t i = 0; i < m_parameters.size(); ++i) {
        m_parameterIndices[m_parameters[i].name] = i;
    }

    // Collect sampler handles for dirty checking
    collectSamplerHandles();
}

Material::~Material() {
    // Smart handles will automatically clean up
}

bool Material::setParameter(const std::string& name, const ParamValue& value) {
    auto it = m_parameterIndices.find(name);
    if (it == m_parameterIndices.end()) {
        SPDLOG_WARN("Material {}: Parameter '{}' not found", m_name, name);
        return false;
    }

    Parameter& param = m_parameters[it->second];

    // Handle texture parameters
    if (std::holds_alternative<TextureParam>(value)) {
        if (!ShaderLib::IsTexture(param.descriptorType)) {
            SPDLOG_WARN("Material {}: Parameter '{}' is not a texture parameter (type: {})",
                m_name, name, ShaderLib::DescriptorTypeToString(param.descriptorType));
            return false;
        }
        param.value = value;
        invalidateDescriptorSet();
        collectSamplerHandles();
        return true;
    }

    // Handle buffer parameters (both UBO and SSBO)
    const ShaderLib::BufferValue* bufVal = std::get_if<ShaderLib::BufferValue>(&value);
    if (!bufVal) {
        SPDLOG_WARN("Material {}: Invalid parameter value type for '{}'", m_name, name);
        return false;
    }

    // Validate that parameter is actually a buffer type
    if (!ShaderLib::IsBuffer(param.descriptorType)) {
        SPDLOG_WARN("Material {}: Parameter '{}' is not a buffer parameter (type: {})",
            m_name, name, ShaderLib::DescriptorTypeToString(param.descriptorType));
        return false;
    }

    // Get expected type from shader metadata
    const auto& metadata = m_shader->metadata;
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        SPDLOG_ERROR("Material {}: No custom descriptor set", m_name);
        return false;
    }

    // Find the buffer variable to get expected type
    const ShaderLib::BufferObject* ownerBuffer = findBufferForVariable(*customSet, param.name);
    if (!ownerBuffer) {
        SPDLOG_WARN("Material {}: Could not find buffer for variable '{}'", m_name, param.name);
        return false;
    }

    // Validate buffer type matches (UBO vs SSBO)
    bool bufferTypeMatches = (param.descriptorType == ShaderLib::DescriptorType::UniformBuffer && ownerBuffer->IsUniformBuffer()) ||
        (param.descriptorType == ShaderLib::DescriptorType::StorageBuffer && ownerBuffer->IsStorageBuffer());

    if (!bufferTypeMatches) {
        SPDLOG_WARN("Material {}: Buffer type mismatch for '{}': parameter type is {}, but buffer is {}",
            m_name, name,
            ShaderLib::DescriptorTypeToString(param.descriptorType),
            ownerBuffer->IsUniformBuffer() ? "UniformBuffer" : "StorageBuffer");
        return false;
    }

    // Find the variable definition
    const ShaderLib::BufferVariable* varDef = nullptr;
    for (const auto& var : ownerBuffer->variables) {
        if (var.name == param.name) {
            varDef = &var;
            break;
        }
    }

    if (!varDef) {
        SPDLOG_WARN("Material {}: Variable '{}' not found in buffer metadata", m_name, param.name);
        return false;
    }

    // Validate type compatibility
    if (varDef->IsComposite()) {
        // For composites, extract from BufferValue
        std::shared_ptr<ShaderLib::CompositeTypeInstance> newInstance;

        if (auto structPtr = std::get_if<std::shared_ptr<ShaderLib::ShaderStructInstance>>(bufVal)) {
            newInstance = std::static_pointer_cast<ShaderLib::CompositeTypeInstance>(*structPtr);
        }
        else if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderLib::ShaderArrayInstance>>(bufVal)) {
            newInstance = std::static_pointer_cast<ShaderLib::CompositeTypeInstance>(*arrayPtr);
        }

        if (!newInstance) {
            SPDLOG_WARN("Material {}: Expected composite type for parameter '{}'", m_name, name);
            return false;
        }

        if (newInstance->GetDefinition()->GetTypeName() != varDef->composite->GetTypeName()) {
            SPDLOG_WARN("Material {}: Composite type mismatch for '{}': expected '{}', got '{}'",
                m_name, name,
                varDef->composite->GetTypeName(),
                newInstance->GetDefinition()->GetTypeName());
            return false;
        }
    }
    else {
        // For base types, check BaseType matches
        ShaderLib::BaseType newType = ShaderLib::GetBaseTypeFromVariant(*bufVal);

        if (newType != varDef->baseType) {
            SPDLOG_WARN("Material {}: Type mismatch for '{}': expected {}, got {}",
                m_name, name,
                ShaderLib::BaseTypeToString(varDef->baseType),
                ShaderLib::BaseTypeToString(newType));
            return false;
        }
    }

    // Type validation passed - update parameter
    param.value = value;
    invalidateDescriptorSet();

    SPDLOG_DEBUG("Material {}: Updated parameter '{}'", m_name, name);
    return true;
}

bool Material::getParameter(const std::string& name, ParamValue& outValue) const {
    auto it = m_parameterIndices.find(name);
    if (it == m_parameterIndices.end()) {
        return false;
    }

    outValue = m_parameters[it->second].value;
    return true;
}

bool Material::readbackBufferParameters() {
    if (!m_shader.isValid()) {
        SPDLOG_ERROR("Material {}: Cannot readback - invalid shader", m_name);
        return false;
    }

    bool anySuccess = false;
    bool anyFailure = false;

    // Read back all buffer parameters
    for (auto& param : m_parameters) {
        if (!param.isBufferParameter()) {
            continue;
        }

        if (readParameterFromBuffer(param)) {
            anySuccess = true;
            SPDLOG_TRACE("Material {}: Successfully read back parameter '{}'", m_name, param.name);
        }
        else {
            anyFailure = true;
            SPDLOG_WARN("Material {}: Failed to read back parameter '{}'", m_name, param.name);
        }
    }

    if (anySuccess && !anyFailure) {
        SPDLOG_DEBUG("Material {}: All buffer parameters read back successfully", m_name);
        return true;
    }
    else if (anySuccess) {
        SPDLOG_WARN("Material {}: Some buffer parameters failed to read back", m_name);
        return true;
    }
    else {
        SPDLOG_ERROR("Material {}: No buffer parameters were read back", m_name);
        return false;
    }
}

bool Material::readbackBufferParameter(const std::string& name) {
    auto it = m_parameterIndices.find(name);
    if (it == m_parameterIndices.end()) {
        SPDLOG_WARN("Material {}: Parameter '{}' not found", m_name, name);
        return false;
    }

    Parameter& param = m_parameters[it->second];

    if (!param.isBufferParameter()) {
        SPDLOG_WARN("Material {}: Parameter '{}' is not a buffer parameter", m_name, name);
        return false;
    }

    return readParameterFromBuffer(param);
}

std::vector<std::string> Material::getBufferParameterNames() const {
    std::vector<std::string> names;

    for (const auto& param : m_parameters) {
        if (param.isBufferParameter()) {
            names.push_back(param.name);
        }
    }

    return names;
}

bool Material::hasBufferParameters() const {
    for (const auto& param : m_parameters) {
        if (param.isBufferParameter()) {
            return true;
        }
    }
    return false;
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> Material::getDescriptorSet() {
    // Check if we need to recreate the descriptor set
    if (needsDescriptorSetRecreation()) {
        m_descriptorSet = createDescriptorSet();

        if (m_descriptorSet.isValid()) {
            m_descriptorSetValid = true;

            // Clear dirty flags for all samplers
            for (SamplerHandle samplerHandle : m_samplerHandles) {
                m_samplerManager.clearDirty(samplerHandle);
            }

            SPDLOG_DEBUG("Material {}: Created descriptor set", m_name);
        }
        else {
            SPDLOG_ERROR("Material {}: Failed to create descriptor set", m_name);
        }
    }

    return m_descriptorSet;
}

void Material::invalidateDescriptorSet() {
    m_descriptorSetValid = false;
    SPDLOG_DEBUG("Material {}: Invalidated descriptor set", m_name);
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> Material::createDescriptorSet() {
    // Get shader metadata
    const auto& metadata = m_shader->metadata;
    const auto& resources = m_shader->resources;

    // Find the custom descriptor set (set 2)
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();
    if (!customSet) {
        SPDLOG_ERROR("Material {}: Shader does not have a custom descriptor set", m_name);
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    // Get the descriptor layout handle for the custom set
    auto layoutIt = resources.descriptorLayouts.find(ShaderLib::CUSTOM_DESCRIPTOR_SET);
    if (layoutIt == resources.descriptorLayouts.end()) {
        SPDLOG_ERROR("Material {}: No descriptor layout found for custom descriptor set", m_name);
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    // Create descriptor set builder
    DescriptorSetBuilder builder(
        m_device,
        m_bufferManager,
        m_samplerManager,
        m_textureManager,
        m_descriptorAllocator,
        m_descriptorLayoutManager
    );

    // Set the descriptor set metadata
    builder.forDescriptorSet(*customSet, layoutIt->second);

    // Bind parameters (optimized to use cached bindings)
    bindParametersToBuilder(builder);

    // Build and return descriptor set
    auto descriptorSet = builder.build();

    if (!descriptorSet.isValid()) {
        SPDLOG_ERROR("Material {}: Failed to build descriptor set", m_name);
    }

    return descriptorSet;
}

bool Material::needsDescriptorSetRecreation() const {
    // Need recreation if descriptor set is invalid or not created yet
    if (!m_descriptorSetValid || !m_descriptorSet.isValid()) {
        return true;
    }

    // Check if any samplers are dirty
    for (SamplerHandle samplerHandle : m_samplerHandles) {
        if (m_samplerManager.isDirty(samplerHandle)) {
            SPDLOG_DEBUG("Material {}: Sampler {} is dirty, need descriptor set recreation",
                m_name, samplerHandle.id);
            return true;
        }
    }

    return false;
}

void Material::collectSamplerHandles() {
    m_samplerHandles.clear();

    for (const auto& param : m_parameters) {
        if (std::holds_alternative<TextureParam>(param.value)) {
            const auto& textureParam = std::get<TextureParam>(param.value);
            if (textureParam.samplerHandle.isValid()) {
                m_samplerHandles.push_back(textureParam.samplerHandle);
            }
        }
    }
}

void Material::bindParametersToBuilder(DescriptorSetBuilder& builder) {
    const auto& metadata = m_shader->metadata;
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        SPDLOG_ERROR("Material {}: No custom descriptor set available", m_name);
        return;
    }

    // Clear previous buffer cache
    m_materialBuffers.clear();

    std::unordered_map<uint32_t, std::pair<std::string, std::unordered_map<std::string, ShaderLib::BufferValue>>> buffersByBinding;

    // First pass: collect ALL buffer variables
    for (const auto& param : m_parameters) {
        if (!ShaderLib::IsBuffer(param.descriptorType)) {
            continue;
        }

        const auto* bufferValue = std::get_if<ShaderLib::BufferValue>(&param.value);
        if (!bufferValue) {
            SPDLOG_WARN("Material {}: Parameter '{}' is marked as buffer but has wrong value type",
                m_name, param.name);
            continue;
        }

        const ShaderLib::BufferObject* ownerBuffer = findBufferForVariable(*customSet, param.name);
        if (!ownerBuffer) {
            SPDLOG_WARN("Material {}: Could not find buffer for variable '{}'", m_name, param.name);
            continue;
        }

        bool bufferTypeMatches = (param.descriptorType == ShaderLib::DescriptorType::UniformBuffer && ownerBuffer->IsUniformBuffer()) ||
            (param.descriptorType == ShaderLib::DescriptorType::StorageBuffer && ownerBuffer->IsStorageBuffer());

        if (!bufferTypeMatches) {
            SPDLOG_WARN("Material {}: Buffer type mismatch for '{}': parameter says {}, buffer is {}",
                m_name, param.name,
                ShaderLib::DescriptorTypeToString(param.descriptorType),
                ownerBuffer->IsUniformBuffer() ? "UniformBuffer" : "StorageBuffer");
            continue;
        }

        auto& bufferData = buffersByBinding[param.binding];
        bufferData.first = ownerBuffer->name;
        bufferData.second[param.name] = *bufferValue;
    }

    // Second pass: create and bind buffers
    for (const auto& [binding, bufferData] : buffersByBinding) {
        const auto& [bufferName, variables] = bufferData;

        const ShaderLib::BufferObject* bufferObj = customSet->GetBuffer(bufferName);
        if (!bufferObj) {
            SPDLOG_ERROR("Material {}: Buffer metadata not found for '{}'", m_name, bufferName);
            continue;
        }

        const char* bufferTypeStr = bufferObj->IsUniformBuffer() ? "UBO" :
            (bufferObj->IsStorageBuffer() ? "SSBO" : "Unknown");
        SPDLOG_DEBUG("Material {}: Binding {} '{}' with {} variables to slot {}",
            m_name, bufferTypeStr, bufferName, variables.size(), binding);

        auto bufferHandle = m_bufferManager.acquireSmartBuffer(*bufferObj);

        // CACHE THE BUFFER HANDLE
        m_materialBuffers[binding] = bufferHandle;

        auto mappedWriter = m_bufferManager.createWriter(bufferHandle.handle());
        if (!mappedWriter.isValid()) {
            SPDLOG_ERROR("Material {}: Failed to map buffer '{}' for writing", m_name, bufferName);
            continue;
        }

        uint32_t updatedCount = 0;
        for (const auto& [varName, value] : variables) {
            bool success = false;

            if (std::holds_alternative<std::shared_ptr<ShaderLib::ShaderStructInstance>>(value)) {
                auto structInstance = std::get<std::shared_ptr<ShaderLib::ShaderStructInstance>>(value);
                success = mappedWriter.writeStruct(varName, structInstance);
            }
            else if (std::holds_alternative<std::shared_ptr<ShaderLib::ShaderArrayInstance>>(value)) {
                auto arrayInstance = std::get<std::shared_ptr<ShaderLib::ShaderArrayInstance>>(value);
                success = mappedWriter.writeArray(varName, arrayInstance);
            }
            else {
                success = mappedWriter.write(varName, value);
            }

            if (success) {
                updatedCount++;
                SPDLOG_TRACE("Material {}: Updated variable '{}' in buffer '{}'",
                    m_name, varName, bufferName);
            }
            else {
                SPDLOG_WARN("Material {}: Failed to write variable '{}' to buffer '{}'",
                    m_name, varName, bufferName);
            }
        }

        SPDLOG_DEBUG("Material {}: Updated {} variables in buffer '{}'", m_name, updatedCount, bufferName);

        builder.bindBufferToSlot(binding, bufferHandle);
    }

    // Third pass: bind textures (unchanged)
    uint32_t textureCount = 0;
    for (const auto& param : m_parameters) {
        if (!ShaderLib::IsTexture(param.descriptorType)) {
            continue;
        }

        const TextureParam* textureParam = std::get_if<TextureParam>(&param.value);
        if (!textureParam) {
            SPDLOG_WARN("Material {}: Parameter '{}' is marked as texture but has wrong value type",
                m_name, param.name);
            continue;
        }

        if (!textureParam->textureHandle.isValid()) {
            SPDLOG_WARN("Material {}: Texture '{}' is not loaded", m_name, param.name);
            continue;
        }

        SPDLOG_DEBUG("Material {}: Binding texture '{}' to slot {}",
            m_name, param.name, param.binding);
        builder.bindTextureToSlot(param.binding, textureParam->textureHandle, textureParam->samplerHandle);
        textureCount++;
    }

    SPDLOG_INFO("Material {}: Bound {} buffer(s) and {} texture(s) to descriptor set",
        m_name, buffersByBinding.size(), textureCount);
}

const ShaderLib::BufferObject* Material::findBufferForVariable(
    const ShaderLib::DescriptorSet& descriptorSet,
    const std::string& variableName
) const {
    for (const auto& [bufferName, buffer] : descriptorSet.buffers) {
        for (const auto& variable : buffer.variables) {
            if (variable.name == variableName) {
                return &buffer;
            }
        }
    }
    return nullptr;
}

bool Material::readParameterFromBuffer(Parameter& param) {
    if (!m_shader.isValid()) {
        SPDLOG_ERROR("Material {}: Invalid shader", m_name);
        return false;
    }

    const auto& metadata = m_shader->metadata;
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        SPDLOG_ERROR("Material {}: No custom descriptor set", m_name);
        return false;
    }

    // Find the buffer this variable belongs to
    const ShaderLib::BufferObject* ownerBuffer = findBufferForVariable(*customSet, param.name);
    if (!ownerBuffer) {
        SPDLOG_WARN("Material {}: Could not find buffer for variable '{}'", m_name, param.name);
        return false;
    }

    // Get or find the buffer handle
    SmartHandle<BufferHandle, Buffer> bufferHandle = getBufferForParameter(param);
    if (!bufferHandle.isValid()) {
        SPDLOG_ERROR("Material {}: Failed to get buffer handle for parameter '{}'", m_name, param.name);
        return false;
    }

    // Create a reader for the buffer
    auto mappedReader = m_bufferManager.createReader(bufferHandle.handle());
    if (!mappedReader.isValid()) {
        SPDLOG_ERROR("Material {}: Failed to map buffer for reading parameter '{}'", m_name, param.name);
        return false;
    }

    // Read the parameter value
    ShaderLib::BufferValue readValue;

    // Find the variable definition
    const ShaderLib::BufferVariable* varDef = nullptr;
    for (const auto& var : ownerBuffer->variables) {
        if (var.name == param.name) {
            varDef = &var;
            break;
        }
    }

    if (!varDef) {
        SPDLOG_WARN("Material {}: Variable '{}' not found in buffer metadata", m_name, param.name);
        return false;
    }

    bool readSuccess = false;

    // Handle composite types
    if (varDef->IsComposite()) {
        std::shared_ptr<ShaderLib::CompositeTypeInstance> instance;

        if (varDef->composite->IsStruct()) {
            std::shared_ptr<ShaderLib::ShaderStructInstance> structInstance;
            readSuccess = mappedReader.readStruct(param.name, structInstance);
            if (readSuccess) {
                readValue = structInstance;
            }
        }
        else if (varDef->composite->IsArray()) {
            std::shared_ptr<ShaderLib::ShaderArrayInstance> arrayInstance;
            readSuccess = mappedReader.readArray(param.name, arrayInstance);
            if (readSuccess) {
                readValue = arrayInstance;
            }
        }
    }
    // Handle base types
    else {
        readSuccess = mappedReader.read(param.name, readValue);
    }

    if (!readSuccess) {
        SPDLOG_WARN("Material {}: Failed to read value for parameter '{}'", m_name, param.name);
        return false;
    }

    // Update the parameter value (without invalidating descriptor set)
    // We only read from GPU, we don't change the descriptor bindings
    param.value = readValue;

    SPDLOG_DEBUG("Material {}: Successfully read back parameter '{}'", m_name, param.name);
    return true;
}

SmartHandle<BufferHandle, Buffer> Material::getBufferForParameter(const Parameter& param) {
    // Check if buffer is in cache
    auto it = m_materialBuffers.find(param.binding);
    if (it != m_materialBuffers.end() && it->second.isValid()) {
        return it->second;
    }

    // Buffer not in cache - descriptor set hasn't been created yet
    // Create it now (this will populate the cache)
    SPDLOG_WARN("Material {}: Buffer for parameter '{}' (binding {}) not in cache. "
        "Creating descriptor set to populate buffer cache.",
        m_name, param.name, param.binding);

    auto descriptorSet = getDescriptorSet();
    if (!descriptorSet.isValid()) {
        SPDLOG_ERROR("Material {}: Failed to create descriptor set for buffer readback", m_name);
        return SmartHandle<BufferHandle, Buffer>();
    }

    // Try again after descriptor set creation
    it = m_materialBuffers.find(param.binding);
    if (it == m_materialBuffers.end() || !it->second.isValid()) {
        SPDLOG_ERROR("Material {}: Buffer for parameter '{}' (binding {}) still not in cache after descriptor set creation",
            m_name, param.name, param.binding);
        return SmartHandle<BufferHandle, Buffer>();
    }

    return it->second;
}