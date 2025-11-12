#include "Material.h"
#include "DescriptorSetBuilder.h"
#include <stdexcept>
#include <spdlog/spdlog.h>
#include <algorithm>

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

    // 1. Initialize buffer instances in bundle
    InitializeBufferBundle(inputBuffer, outputBuffer, inputOutputBuffer);

    // 2. Build texture bindings from shader metadata
    BuildTextureBindings();
    CollectSamplerHandles();

    // 3. IMMEDIATELY acquire GPU buffers and connect them to buffer instances
    // This ensures buffers are always available for sync operations
    AcquireGPUBuffers();

    uint32_t bufferCount = m_bufferBundle.GetBufferCount();

    SPDLOG_INFO("Material '{}': Created with {} GPU buffer(s) and {} texture binding(s)",
        m_name, bufferCount, m_textureBindings.size());
}

Material::~Material() {
    // Release GPU buffers (SmartHandles will automatically decrement ref count)
    m_inputBufferHandle = SmartHandle<BufferHandle, Buffer>();
    m_outputBufferHandle = SmartHandle<BufferHandle, Buffer>();
    m_inputOutputBufferHandle = SmartHandle<BufferHandle, Buffer>();
}

// =============================================================================
// INITIALIZATION
// =============================================================================

void Material::InitializeBufferBundle(
    std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer,
    std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer,
    std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer
) {
    // Add buffers to bundle with standard identifiers
    if (inputBuffer) {
        m_bufferBundle.AddBuffer("input", inputBuffer, ShaderLib::INPUT_DATA_BINDING);
    }

    if (outputBuffer) {
        m_bufferBundle.AddBuffer("output", outputBuffer, ShaderLib::OUTPUT_DATA_BINDING);
    }

    if (inputOutputBuffer) {
        m_bufferBundle.AddBuffer("input_output", inputOutputBuffer, ShaderLib::INPUT_OUTPUT_DATA_BINDING);
    }
}

void Material::BuildTextureBindings() {
    const ShaderLib::DescriptorSet* customSet = GetCustomDescriptorSet();
    if (!customSet) {
        return;
    }

    m_textureBindings.clear();

    for (const auto& slot : customSet->slots) {
        if (slot.IsSampler()) {
            TextureBinding texBinding;
            texBinding.name = slot.name;
            texBinding.binding = slot.binding;
            texBinding.type = slot.type;
            m_textureBindings[texBinding.name] = texBinding;
        }
    }

    SPDLOG_DEBUG("Material '{}': Built {} texture binding(s)", m_name, m_textureBindings.size());
}

void Material::CollectSamplerHandles() {
    m_samplerHandles.clear();

    for (const auto& [name, binding] : m_textureBindings) {
        if (binding.texture.samplerHandle.isValid()) {
            m_samplerHandles.push_back(binding.texture.samplerHandle);
        }
    }
}

void Material::AcquireGPUBuffers() {
    // Acquire GPU buffers for all buffer instances
    // These buffers are acquired immediately and stay valid for the material's lifetime

    auto inputBuffer = m_bufferBundle.GetBuffer("input");
    if (inputBuffer) {
        m_inputBufferHandle = m_bufferManager.acquireSmartBuffer(
            inputBuffer->GetDefinition()
        );

        if (m_inputBufferHandle.isValid()) {
            // Connect buffer instance to GPU buffer
            inputBuffer->SetMappedBuffer(m_bufferManager.getResource(m_inputBufferHandle.handle()));
            SPDLOG_DEBUG("Material '{}': Acquired GPU buffer for input (size: {} bytes)",
                m_name, inputBuffer->GetDefinition()->GetTotalSize());
        }
        else {
            throw std::runtime_error("Material " + m_name + ": Failed to acquire input GPU buffer");
        }
    }

    auto outputBuffer = m_bufferBundle.GetBuffer("output");
    if (outputBuffer) {
        m_outputBufferHandle = m_bufferManager.acquireSmartBuffer(
            outputBuffer->GetDefinition()
        );

        if (m_outputBufferHandle.isValid()) {
            outputBuffer->SetMappedBuffer(m_bufferManager.getResource(m_outputBufferHandle.handle()));
            SPDLOG_DEBUG("Material '{}': Acquired GPU buffer for output (size: {} bytes)",
                m_name, outputBuffer->GetDefinition()->GetTotalSize());
        }
        else {
            throw std::runtime_error("Material " + m_name + ": Failed to acquire output GPU buffer");
        }
    }

    auto inputOutputBuffer = m_bufferBundle.GetBuffer("input_output");
    if (inputOutputBuffer) {
        m_inputOutputBufferHandle = m_bufferManager.acquireSmartBuffer(
            inputOutputBuffer->GetDefinition()
        );

        if (m_inputOutputBufferHandle.isValid()) {
            inputOutputBuffer->SetMappedBuffer(m_bufferManager.getResource(m_inputOutputBufferHandle.handle()));
            SPDLOG_DEBUG("Material '{}': Acquired GPU buffer for input_output (size: {} bytes)",
                m_name, inputOutputBuffer->GetDefinition()->GetTotalSize());
        }
        else {
            throw std::runtime_error("Material " + m_name + ": Failed to acquire input_output GPU buffer");
        }
    }
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
// DESCRIPTOR SET MANAGEMENT (LAZY CREATION)
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

    // GPU buffers are already acquired in constructor - just bind them
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

    // Bind input buffer (already acquired)
    if (m_inputBufferHandle.isValid()) {
        builder.bindBufferToSlot(ShaderLib::INPUT_DATA_BINDING, m_inputBufferHandle);
        bufferCount++;
    }

    // Bind output buffer (already acquired)
    if (m_outputBufferHandle.isValid()) {
        builder.bindBufferToSlot(ShaderLib::OUTPUT_DATA_BINDING, m_outputBufferHandle);
        bufferCount++;
    }

    // Bind input/output buffer (already acquired)
    if (m_inputOutputBufferHandle.isValid()) {
        builder.bindBufferToSlot(ShaderLib::INPUT_OUTPUT_DATA_BINDING, m_inputOutputBufferHandle);
        bufferCount++;
    }

    // Bind textures
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

// =============================================================================
// HELPER METHODS
// =============================================================================

const ShaderLib::DescriptorSet* Material::GetCustomDescriptorSet() const {
    if (!m_shader.isValid()) {
        return nullptr;
    }

    return m_shader->metadata.GetCustomSet();
}

// =============================================================================
// BULK BUFFER OPERATIONS
// GPU buffers are always available - no initialization checks needed
// =============================================================================

void Material::SyncAllToGPU() {
    for (const auto& identifier : m_bufferBundle.GetBufferIdentifiers()) {
        auto buffer = m_bufferBundle.GetBuffer(identifier);
        if (buffer) {
            buffer->SyncToBuffer();
        }
    }

    SPDLOG_TRACE("Material '{}': Synced all buffers to GPU", m_name);
}

void Material::SyncAllFromGPU() {
    for (const auto& identifier : m_bufferBundle.GetBufferIdentifiers()) {
        auto buffer = m_bufferBundle.GetBuffer(identifier);
        if (buffer) {
            buffer->SyncFromBuffer();
        }
    }

    SPDLOG_TRACE("Material '{}': Synced all buffers from GPU", m_name);
}

std::vector<ShaderLib::AsyncOperationHandle> Material::SyncAllToGPUAsync() {
    std::vector<ShaderLib::AsyncOperationHandle> handles;

    for (const auto& identifier : m_bufferBundle.GetBufferIdentifiers()) {
        auto buffer = m_bufferBundle.GetBuffer(identifier);
        if (buffer) {
            auto handle = buffer->SyncToBufferAsync();
            if (handle.isValid()) {
                handles.push_back(handle);
            }
        }
    }

    SPDLOG_TRACE("Material '{}': Started async sync to GPU ({} operations)", m_name, handles.size());
    return handles;
}

std::vector<ShaderLib::AsyncOperationHandle> Material::SyncAllFromGPUAsync() {
    std::vector<ShaderLib::AsyncOperationHandle> handles;

    for (const auto& identifier : m_bufferBundle.GetBufferIdentifiers()) {
        auto buffer = m_bufferBundle.GetBuffer(identifier);
        if (buffer) {
            auto handle = buffer->SyncFromBufferAsync();
            if (handle.isValid()) {
                handles.push_back(handle);
            }
        }
    }

    SPDLOG_TRACE("Material '{}': Started async sync from GPU ({} operations)", m_name, handles.size());
    return handles;
}

void Material::WaitForAllBuffers() {
    for (const auto& identifier : m_bufferBundle.GetBufferIdentifiers()) {
        auto buffer = m_bufferBundle.GetBuffer(identifier);
        if (buffer) {
            buffer->WaitForAllSyncs();
        }
    }

    SPDLOG_TRACE("Material '{}': Waited for all buffer operations", m_name);
}
