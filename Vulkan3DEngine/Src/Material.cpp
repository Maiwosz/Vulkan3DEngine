#include "Material.h"
#include "DescriptorSetBuilder.h"
#include <stdexcept>
#include <spdlog/spdlog.h>
#include <algorithm>

// =============================================================================
// FIELD MAPPING HELPER
// =============================================================================

std::shared_ptr<ShaderLib::BufferObjectInstance>
Material::FieldMapping::GetBufferInstance(
    const std::shared_ptr<ShaderLib::BufferObjectInstance>& input,
    const std::shared_ptr<ShaderLib::BufferObjectInstance>& output,
    const std::shared_ptr<ShaderLib::BufferObjectInstance>& inputOutput
) const {
    switch (binding) {
    case ShaderLib::INPUT_DATA_BINDING:
        return input;
    case ShaderLib::OUTPUT_DATA_BINDING:
        return output;
    case ShaderLib::INPUT_OUTPUT_DATA_BINDING:
        return inputOutput;
    default:
        return nullptr;
    }
}

// =============================================================================
// CONSTRUCTOR / DESTRUCTOR
// =============================================================================

Material::Material(
    const std::string& name,
    SmartAssetHandle<ShaderHandle, ShaderAsset> shader,
    std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer,
    std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer,
    std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer,
    const LogicalDevice& device,
    BufferManager& bufferManager,
    ImageSamplerManager& samplerManager,
    TextureManager& textureManager,
    DescriptorAllocator& descriptorAllocator,
    DescriptorLayoutManager& descriptorLayoutManager
)
    : m_name(name)
    , m_shader(shader)
    , m_inputBuffer(inputBuffer)
    , m_outputBuffer(outputBuffer)
    , m_inputOutputBuffer(inputOutputBuffer)
    , m_device(device)
    , m_bufferManager(bufferManager)
    , m_samplerManager(samplerManager)
    , m_textureManager(textureManager)
    , m_descriptorAllocator(descriptorAllocator)
    , m_descriptorLayoutManager(descriptorLayoutManager)
    , m_descriptorSetValid(false)
{
    if (!m_shader.isValid()) {
        throw std::runtime_error("Material " + m_name + ": Invalid shader");
    }

    BuildFieldMappings();
    CollectSamplerHandles();

    SPDLOG_INFO("Material '{}': Created with {} field mappings and {} texture bindings",
        m_name, m_fieldMappings.size(), m_textureBindings.size());
}

Material::~Material() {
    ReleaseDescriptorSetBuffers();
}

// =============================================================================
// INITIALIZATION
// =============================================================================

void Material::BuildFieldMappings() {
    const ShaderLib::DescriptorSet* customSet = GetCustomDescriptorSet();
    if (!customSet) {
        return;
    }

    m_fieldMappings.clear();
    m_topLevelToPaths.clear();
    m_fieldInfoCache.clear();
    m_textureBindings.clear();

    // Process buffer fields
    for (const auto& slot : customSet->slots) {
        if (slot.IsBuffer()) {
            auto bufferDef = customSet->GetBufferByBinding(slot.binding);
            if (!bufferDef) continue;

            // Get buffer instance
            std::shared_ptr<ShaderLib::BufferObjectInstance> bufferInstance = nullptr;
            switch (slot.binding) {
            case ShaderLib::INPUT_DATA_BINDING:
                bufferInstance = m_inputBuffer;
                break;
            case ShaderLib::OUTPUT_DATA_BINDING:
                bufferInstance = m_outputBuffer;
                break;
            case ShaderLib::INPUT_OUTPUT_DATA_BINDING:
                bufferInstance = m_inputOutputBuffer;
                break;
            }

            if (!bufferInstance) continue;

            // Get ALL fields (including structures)
            const auto& allFields = bufferDef->GetAllFields();

            for (const auto& field : allFields) {
                // Map ALL fields (structures and base types)
                FieldMapping mapping;
                mapping.fieldName = field.name;
                mapping.fullPath = field.path;
                mapping.bufferType = bufferDef->GetBufferType();
                mapping.binding = slot.binding;
                mapping.isBaseType = field.isBaseType;
                mapping.isArray = field.isArray;
                mapping.arraySize = field.arraySize;

                // Add to main mapping (by full path)
                m_fieldMappings[field.path] = mapping;

                // Build field info cache
                FieldInfo info;
                info.name = field.name;
                info.path = field.path;
                info.baseType = field.baseType;
                info.binding = slot.binding;
                info.isBaseType = field.isBaseType;
                info.isArray = field.isArray;
                info.arraySize = field.arraySize;
                info.offset = field.offset;
                info.size = field.size;

                m_fieldInfoCache[field.path] = info;

                // Extract top-level name for quick lookup
                std::string topLevelName = ExtractTopLevelName(field.path);
                m_topLevelToPaths[topLevelName].push_back(field.path);
            }
        }
        else if (slot.IsSampler()) {
            // Map textures
            TextureBinding texBinding;
            texBinding.name = slot.name;
            texBinding.binding = slot.binding;
            texBinding.type = slot.type;
            m_textureBindings[texBinding.name] = texBinding;
        }
    }

    SPDLOG_DEBUG("Material '{}': Built {} field mappings ({} top-level fields)",
        m_name, m_fieldMappings.size(), m_topLevelToPaths.size());
}

void Material::CollectSamplerHandles() {
    m_samplerHandles.clear();

    for (const auto& [name, binding] : m_textureBindings) {
        if (binding.texture.samplerHandle.isValid()) {
            m_samplerHandles.push_back(binding.texture.samplerHandle);
        }
    }
}

// =============================================================================
// FIELD ACCESS - WITH CHAINING SUPPORT
// =============================================================================

ShaderLib::FieldProxy Material::operator[](const std::string& name) {
    // Check if this is a path (contains . or [)
    if (name.find('.') != std::string::npos || name.find('[') != std::string::npos) {
        // Path notation - delegate to GetField
        return GetField(name);
    }

    // Top-level field access - find the buffer and delegate
    auto it = m_topLevelToPaths.find(name);
    if (it == m_topLevelToPaths.end() || it->second.empty()) {
        throw std::runtime_error("Material " + m_name + ": Field not found: " + name);
    }

    // Use first path (base case without array index if it's an array)
    const std::string& firstPath = it->second[0];
    const FieldMapping* mapping = FindFieldMapping(firstPath);
    if (!mapping) {
        throw std::runtime_error("Material " + m_name + ": Field mapping not found: " + name);
    }

    auto buffer = mapping->GetBufferInstance(m_inputBuffer, m_outputBuffer, m_inputOutputBuffer);
    if (!buffer) {
        throw std::runtime_error("Material " + m_name + ": Buffer not found for field: " + name);
    }

    // Return FieldProxy for the top-level field (supports chaining)
    return (*buffer)[name];
}

ShaderLib::FieldProxy Material::operator[](const char* name) {
    return operator[](std::string(name));
}

ShaderLib::FieldProxy Material::GetField(const std::string& path) {
    auto buffer = GetBufferForField(path);
    if (!buffer) {
        throw std::runtime_error("Material " + m_name + ": Field not found: " + path);
    }

    return buffer->GetField(path);
}

bool Material::HasField(const std::string& nameOrPath) const {
    // Try as path first
    if (m_fieldMappings.find(nameOrPath) != m_fieldMappings.end()) {
        return true;
    }

    // Try as top-level name
    if (m_topLevelToPaths.find(nameOrPath) != m_topLevelToPaths.end()) {
        return true;
    }

    return false;
}

std::vector<std::string> Material::GetFieldNames() const {
    std::vector<std::string> names;
    names.reserve(m_topLevelToPaths.size());

    for (const auto& [name, _] : m_topLevelToPaths) {
        names.push_back(name);
    }

    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> Material::GetAllFieldPaths() const {
    std::vector<std::string> paths;
    paths.reserve(m_fieldMappings.size());

    for (const auto& [path, _] : m_fieldMappings) {
        paths.push_back(path);
    }

    std::sort(paths.begin(), paths.end());
    return paths;
}

const Material::FieldInfo* Material::GetFieldInfo(const std::string& nameOrPath) const {
    // Try as path first
    auto it = m_fieldInfoCache.find(nameOrPath);
    if (it != m_fieldInfoCache.end()) {
        return &it->second;
    }

    // Try as top-level name
    auto topIt = m_topLevelToPaths.find(nameOrPath);
    if (topIt != m_topLevelToPaths.end() && !topIt->second.empty()) {
        const std::string& firstPath = topIt->second[0];
        auto infoIt = m_fieldInfoCache.find(firstPath);
        if (infoIt != m_fieldInfoCache.end()) {
            return &infoIt->second;
        }
    }

    return nullptr;
}

// =============================================================================
// ARRAY OPERATIONS
// =============================================================================

bool Material::IsArrayField(const std::string& name) const {
    const FieldInfo* info = GetFieldInfo(name);
    return info && info->isArray;
}

size_t Material::GetArraySize(const std::string& name) const {
    const FieldInfo* info = GetFieldInfo(name);
    return info ? info->arraySize : 0;
}

// =============================================================================
// STRUCTURE OPERATIONS
// =============================================================================

bool Material::IsStructureField(const std::string& name) const {
    const FieldInfo* info = GetFieldInfo(name);
    return info && !info->isBaseType;
}

std::vector<std::string> Material::GetStructureChildren(const std::string& name) const {
    if (!IsStructureField(name)) {
        return {};
    }

    std::vector<std::string> children;
    std::string prefix = name + ".";

    // Find all paths that start with "name."
    for (const auto& [path, _] : m_fieldMappings) {
        if (path.find(prefix) == 0) {
            // Extract immediate child name (without further nesting)
            std::string remainder = path.substr(prefix.length());
            size_t dotPos = remainder.find('.');
            size_t bracketPos = remainder.find('[');
            size_t endPos = std::min(dotPos, bracketPos);

            std::string childName = (endPos != std::string::npos)
                ? remainder.substr(0, endPos)
                : remainder;

            // Add unique child names
            if (std::find(children.begin(), children.end(), childName) == children.end()) {
                children.push_back(childName);
            }
        }
    }

    std::sort(children.begin(), children.end());
    return children;
}

// =============================================================================
// TEXTURE MANAGEMENT
// =============================================================================

bool Material::SetTexture(const std::string& name, const TextureParam& texture) {
    auto it = m_textureBindings.find(name);
    if (it == m_textureBindings.end()) {
        SPDLOG_WARN("Material '{}': Texture binding '{}' not found", m_name, name);
        return false;
    }

    it->second.texture = texture;
    InvalidateDescriptorSet();
    CollectSamplerHandles();

    SPDLOG_DEBUG("Material '{}': Set texture '{}'", m_name, name);
    return true;
}

bool Material::GetTexture(const std::string& name, TextureParam& outTexture) const {
    auto it = m_textureBindings.find(name);
    if (it == m_textureBindings.end()) {
        return false;
    }

    outTexture = it->second.texture;
    return true;
}

std::vector<std::string> Material::GetTextureNames() const {
    std::vector<std::string> names;
    names.reserve(m_textureBindings.size());

    for (const auto& [name, _] : m_textureBindings) {
        names.push_back(name);
    }

    std::sort(names.begin(), names.end());
    return names;
}

bool Material::HasTexture(const std::string& name) const {
    return m_textureBindings.find(name) != m_textureBindings.end();
}

// =============================================================================
// GPU SYNCHRONIZATION
// =============================================================================

void Material::SyncToGPU() {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }

    if (m_inputBuffer) m_inputBuffer->SyncToBuffer();
    if (m_outputBuffer) m_outputBuffer->SyncToBuffer();
    if (m_inputOutputBuffer) m_inputOutputBuffer->SyncToBuffer();

    SPDLOG_TRACE("Material '{}': Synced all buffers to GPU", m_name);
}

void Material::SyncFromGPU() {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }

    if (m_inputBuffer) m_inputBuffer->SyncFromBuffer();
    if (m_outputBuffer) m_outputBuffer->SyncFromBuffer();
    if (m_inputOutputBuffer) m_inputOutputBuffer->SyncFromBuffer();

    SPDLOG_TRACE("Material '{}': Synced all buffers from GPU", m_name);
}

// =============================================================================
// DESCRIPTOR SET MANAGEMENT
// =============================================================================

SmartHandle<DescriptorSetHandle, VkDescriptorSet> Material::GetDescriptorSet() {
    if (NeedsDescriptorSetRecreation()) {
        m_descriptorSet = CreateDescriptorSet();

        if (m_descriptorSet.isValid()) {
            m_descriptorSetValid = true;

            for (SamplerHandle samplerHandle : m_samplerHandles) {
                m_samplerManager.clearDirty(samplerHandle);
            }

            SPDLOG_DEBUG("Material '{}': Created descriptor set", m_name);
        }
        else {
            SPDLOG_ERROR("Material '{}': Failed to create descriptor set", m_name);
        }
    }

    return m_descriptorSet;
}

void Material::InvalidateDescriptorSet() {
    m_descriptorSetValid = false;
    SPDLOG_DEBUG("Material '{}': Invalidated descriptor set", m_name);
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> Material::CreateDescriptorSet() {
    const auto& metadata = m_shader->metadata;
    const auto& resources = m_shader->resources;

    const ShaderLib::DescriptorSet* customSet = GetCustomDescriptorSet();
    if (!customSet) {
        SPDLOG_ERROR("Material '{}': Shader does not have a custom descriptor set", m_name);
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    auto layoutIt = resources.descriptorLayouts.find(ShaderLib::CUSTOM_DESCRIPTOR_SET);
    if (layoutIt == resources.descriptorLayouts.end()) {
        SPDLOG_ERROR("Material '{}': No descriptor layout found for custom descriptor set", m_name);
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    AcquireBuffersForDescriptorSet();

    DescriptorSetBuilder builder(
        m_device,
        m_bufferManager,
        m_samplerManager,
        m_textureManager,
        m_descriptorAllocator,
        m_descriptorLayoutManager
    );

    builder.forDescriptorSet(*customSet, layoutIt->second);

    uint32_t bufferCount = 0;

    if (m_inputBufferHandle.isValid()) {
        builder.bindBufferToSlot(ShaderLib::INPUT_DATA_BINDING, m_inputBufferHandle);
        if (m_inputBuffer) {
            m_inputBuffer->SetMappedBuffer(m_bufferManager.getResource(m_inputBufferHandle.handle()));
        }
        bufferCount++;
    }

    if (m_outputBufferHandle.isValid()) {
        builder.bindBufferToSlot(ShaderLib::OUTPUT_DATA_BINDING, m_outputBufferHandle);
        if (m_outputBuffer) {
            m_outputBuffer->SetMappedBuffer(m_bufferManager.getResource(m_outputBufferHandle.handle()));
        }
        bufferCount++;
    }

    if (m_inputOutputBufferHandle.isValid()) {
        builder.bindBufferToSlot(ShaderLib::INPUT_OUTPUT_DATA_BINDING, m_inputOutputBufferHandle);
        if (m_inputOutputBuffer) {
            m_inputOutputBuffer->SetMappedBuffer(m_bufferManager.getResource(m_inputOutputBufferHandle.handle()));
        }
        bufferCount++;
    }

    uint32_t textureCount = 0;
    for (const auto& [name, binding] : m_textureBindings) {
        if (!binding.texture.textureHandle.isValid()) {
            SPDLOG_WARN("Material '{}': Texture '{}' is not loaded", m_name, name);
            continue;
        }

        builder.bindTextureToSlot(binding.binding, binding.texture.textureHandle,
            binding.texture.samplerHandle);
        textureCount++;
    }

    auto descriptorSet = builder.build();

    if (!descriptorSet.isValid()) {
        SPDLOG_ERROR("Material '{}': Failed to build descriptor set", m_name);
        ReleaseDescriptorSetBuffers();
    }
    else {
        SPDLOG_INFO("Material '{}': Created descriptor set with {} buffer(s) and {} texture(s)",
            m_name, bufferCount, textureCount);
    }

    return descriptorSet;
}

bool Material::NeedsDescriptorSetRecreation() const {
    if (!m_descriptorSetValid || !m_descriptorSet.isValid()) {
        return true;
    }

    for (SamplerHandle samplerHandle : m_samplerHandles) {
        if (m_samplerManager.isDirty(samplerHandle)) {
            SPDLOG_DEBUG("Material '{}': Sampler {} is dirty, need descriptor set recreation",
                m_name, samplerHandle.id);
            return true;
        }
    }

    return false;
}

void Material::AcquireBuffersForDescriptorSet() {
    ReleaseDescriptorSetBuffers();

    if (m_inputBuffer) {
        m_inputBufferHandle = m_bufferManager.acquireSmartBuffer(
            m_inputBuffer->GetDefinition()
        );
    }

    if (m_outputBuffer) {
        m_outputBufferHandle = m_bufferManager.acquireSmartBuffer(
            m_outputBuffer->GetDefinition()
        );
    }

    if (m_inputOutputBuffer) {
        m_inputOutputBufferHandle = m_bufferManager.acquireSmartBuffer(
            m_inputOutputBuffer->GetDefinition()
        );
    }
}

void Material::ReleaseDescriptorSetBuffers() {
    m_inputBufferHandle = SmartHandle<BufferHandle, Buffer>();
    m_outputBufferHandle = SmartHandle<BufferHandle, Buffer>();
    m_inputOutputBufferHandle = SmartHandle<BufferHandle, Buffer>();
}

// =============================================================================
// HELPER METHODS
// =============================================================================

const Material::FieldMapping* Material::FindFieldMapping(const std::string& nameOrPath) const {
    // Try as path first
    auto it = m_fieldMappings.find(nameOrPath);
    if (it != m_fieldMappings.end()) {
        return &it->second;
    }

    // Try as top-level name
    auto topIt = m_topLevelToPaths.find(nameOrPath);
    if (topIt != m_topLevelToPaths.end() && !topIt->second.empty()) {
        const std::string& firstPath = topIt->second[0];
        auto mappingIt = m_fieldMappings.find(firstPath);
        if (mappingIt != m_fieldMappings.end()) {
            return &mappingIt->second;
        }
    }

    return nullptr;
}

std::string Material::ExtractTopLevelName(const std::string& path) const {
    // Extract name before first '.' or '['
    size_t dotPos = path.find('.');
    size_t bracketPos = path.find('[');
    size_t endPos = std::min(dotPos, bracketPos);

    if (endPos == std::string::npos) {
        return path;
    }

    return path.substr(0, endPos);
}

const ShaderLib::DescriptorSet* Material::GetCustomDescriptorSet() const {
    if (!m_shader.isValid()) {
        return nullptr;
    }

    return m_shader->metadata.GetCustomSet();
}

std::shared_ptr<ShaderLib::BufferObjectInstance> Material::GetBufferForField(const std::string& path) const {
    const FieldMapping* mapping = FindFieldMapping(path);
    if (!mapping) {
        return nullptr;
    }

    return mapping->GetBufferInstance(m_inputBuffer, m_outputBuffer, m_inputOutputBuffer);
}

std::vector<std::string> Material::CollectStructurePaths(const std::string& structureName) const {
    std::vector<std::string> paths;

    // Find the structure field info
    const FieldInfo* info = GetFieldInfo(structureName);
    if (!info || info->isBaseType) {
        return paths;
    }

    // Get all paths that start with "structureName."
    std::string prefix = structureName + ".";

    for (const auto& [path, _] : m_fieldMappings) {
        if (path.find(prefix) == 0) {
            paths.push_back(path);
        }
    }

    // Also include the structure itself if it exists
    if (m_fieldMappings.find(structureName) != m_fieldMappings.end()) {
        paths.insert(paths.begin(), structureName);
    }

    std::sort(paths.begin(), paths.end());
    return paths;
}

std::vector<std::string> Material::CollectArrayPaths(const std::string& arrayName) const {
    std::vector<std::string> paths;

    // Find the array field info
    const FieldInfo* info = GetFieldInfo(arrayName);
    if (!info || !info->isArray) {
        return paths;
    }

    // Generate all array element paths
    for (uint32_t i = 0; i < info->arraySize; ++i) {
        std::string elementPath = arrayName + "[" + std::to_string(i) + "]";

        // Check if this path exists
        if (m_fieldMappings.find(elementPath) != m_fieldMappings.end()) {
            paths.push_back(elementPath);

            // If array of structures, also include nested paths
            const FieldInfo* elementInfo = GetFieldInfo(elementPath);
            if (elementInfo && !elementInfo->isBaseType) {
                auto nestedPaths = CollectStructurePaths(elementPath);
                paths.insert(paths.end(), nestedPaths.begin(), nestedPaths.end());
            }
        }
    }

    std::sort(paths.begin(), paths.end());
    return paths;
}
