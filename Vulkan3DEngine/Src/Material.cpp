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
    const std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>& buffers,
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

    // 1. Build buffer entries with bindings from shader metadata
    BuildBufferEntries(buffers);

    // 2. Build texture bindings from shader metadata
    BuildTextureBindings();
    CollectSamplerHandles();

    // 3. IMMEDIATELY acquire GPU buffers and connect them to buffer instances
    AcquireGPUBuffers();

    SPDLOG_INFO("Material '{}': Created with {} GPU buffer(s) and {} texture binding(s)",
        m_name, m_buffers.size(), m_textureBindings.size());
}

Material::~Material() {
    // GPU buffer handles are automatically released via SmartHandle
}

// =============================================================================
// INITIALIZATION
// =============================================================================

void Material::BuildBufferEntries(
    const std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>& buffers
) {
    const ShaderLib::DescriptorSet* customSet = GetCustomDescriptorSet();
    if (!customSet) {
        return;
    }

    // Build buffer entries from shader metadata + provided instances
    for (const auto& slot : customSet->slots) {
        if (!slot.IsBuffer()) {
            continue;
        }

        // Find buffer instance by slot name
        auto it = buffers.find(slot.name);
        if (it == buffers.end() || !it->second) {
            SPDLOG_WARN("Material '{}': Buffer '{}' not provided", m_name, slot.name);
            continue;
        }

        BufferEntry entry;
        entry.name = slot.name;
        entry.binding = slot.binding;
        entry.instance = it->second;

        m_buffers[slot.name] = std::move(entry);

        SPDLOG_DEBUG("Material '{}': Registered buffer '{}' at binding {}",
            m_name, slot.name, slot.binding);
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
    for (auto& [name, entry] : m_buffers) {
        if (!entry.instance) {
            continue;
        }

        // Acquire GPU buffer for this instance
        entry.gpuHandle = m_bufferManager.acquireSmartBuffer(
            entry.instance->GetDefinition()
        );

        if (entry.gpuHandle.isValid()) {
            // Connect buffer instance to GPU buffer
            entry.instance->SetMappedBuffer(m_bufferManager.getResource(entry.gpuHandle.handle()));
            SPDLOG_DEBUG("Material '{}': Acquired GPU buffer for '{}' (size: {} bytes)",
                m_name, name, entry.instance->GetDefinition()->GetTotalSize());
        }
        else {
            throw std::runtime_error("Material " + m_name +
                ": Failed to acquire GPU buffer for '" + name + "'");
        }
    }
}

// =============================================================================
// BUFFER ACCESS BY NAME
// =============================================================================

bool Material::HasBuffer(const std::string& name) const {
    return m_buffers.find(name) != m_buffers.end();
}

std::shared_ptr<ShaderLib::BufferObjectInstance> Material::GetBuffer(const std::string& name) {
    auto it = m_buffers.find(name);
    return it != m_buffers.end() ? it->second.instance : nullptr;
}

std::shared_ptr<const ShaderLib::BufferObjectInstance> Material::GetBuffer(const std::string& name) const {
    auto it = m_buffers.find(name);
    return it != m_buffers.end() ? it->second.instance : nullptr;
}

std::vector<std::string> Material::GetBufferNames() const {
    std::vector<std::string> names;
    names.reserve(m_buffers.size());

    for (const auto& [name, _] : m_buffers) {
        names.push_back(name);
    }

    std::sort(names.begin(), names.end());
    return names;
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

    DescriptorSetBuilder builder(
        m_device,
        m_bufferManager,
        m_samplerManager,
        m_textureManager,
        m_descriptorAllocator,
        m_descriptorLayoutManager
    );

    builder.forDescriptorSet(*customSet, layoutIt->second);

    // Bind all buffers
    uint32_t bufferCount = 0;
    for (const auto& [name, entry] : m_buffers) {
        if (entry.gpuHandle.isValid()) {
            builder.bindBufferToSlot(entry.binding, entry.gpuHandle);
            bufferCount++;
        }
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
    for (const auto& [name, entry] : m_buffers) {
        if (entry.instance) {
            entry.instance->SyncToBuffer();
        }
    }

    SPDLOG_TRACE("Material '{}': Synced all buffers to GPU", m_name);
}

void Material::SyncAllFromGPU() {
    for (const auto& [name, entry] : m_buffers) {
        if (entry.instance) {
            entry.instance->SyncFromBuffer();
        }
    }

    SPDLOG_TRACE("Material '{}': Synced all buffers from GPU", m_name);
}

std::vector<ShaderLib::AsyncOperationHandle> Material::SyncAllToGPUAsync() {
    std::vector<ShaderLib::AsyncOperationHandle> handles;

    for (const auto& [name, entry] : m_buffers) {
        if (entry.instance) {
            auto handle = entry.instance->SyncToBufferAsync();
            if (handle.isValid()) {
                handles.push_back(handle);
            }
        }
    }

    SPDLOG_TRACE("Material '{}': Started async sync to GPU ({} operations)",
        m_name, handles.size());
    return handles;
}

std::vector<ShaderLib::AsyncOperationHandle> Material::SyncAllFromGPUAsync() {
    std::vector<ShaderLib::AsyncOperationHandle> handles;

    for (const auto& [name, entry] : m_buffers) {
        if (entry.instance) {
            auto handle = entry.instance->SyncFromBufferAsync();
            if (handle.isValid()) {
                handles.push_back(handle);
            }
        }
    }

    SPDLOG_TRACE("Material '{}': Started async sync from GPU ({} operations)",
        m_name, handles.size());
    return handles;
}

void Material::WaitForAllBuffers() {
    for (const auto& [name, entry] : m_buffers) {
        if (entry.instance) {
            entry.instance->WaitForAllSyncs();
        }
    }

    SPDLOG_TRACE("Material '{}': Waited for all buffer operations", m_name);
}
