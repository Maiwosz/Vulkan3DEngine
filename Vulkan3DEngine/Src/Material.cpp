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
    DescriptorLayoutManager& descriptorLayoutManager,
    ThreadPool& threadPool
)
    : m_name(name)
    , m_shader(shader)
    , m_bufferBundle(threadPool)
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

    // Register buffers in BufferBundle (BufferBundle builds all caches automatically)
    if (inputBuffer) {
        m_bufferBundle.AddBuffer("input", inputBuffer, ShaderLib::INPUT_DATA_BINDING);
    }
    if (outputBuffer) {
        m_bufferBundle.AddBuffer("output", outputBuffer, ShaderLib::OUTPUT_DATA_BINDING);
    }
    if (inputOutputBuffer) {
        m_bufferBundle.AddBuffer("inputOutput", inputOutputBuffer, ShaderLib::INPUT_OUTPUT_DATA_BINDING);
    }

    // Build texture-specific caches (Material's responsibility)
    BuildTextureBindings();
    CollectSamplerHandles();

    SPDLOG_INFO("Material '{}': Created with {} buffers, {} fields and {} texture bindings",
        m_name, m_bufferBundle.GetBufferCount(),
        m_bufferBundle.GetAllFieldPaths().size(), m_textureBindings.size());
}

Material::~Material() {
    // BufferBundle handles waiting for all tasks
    ReleaseDescriptorSetBuffers();
}

// =============================================================================
// INITIALIZATION
// =============================================================================

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

    SPDLOG_DEBUG("Material '{}': Built {} texture bindings", m_name, m_textureBindings.size());
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
        return GetField(name);
    }

    // Top-level field access - delegate to BufferBundle
    return m_bufferBundle.GetField(name);
}

ShaderLib::FieldProxy Material::operator[](const char* name) {
    return operator[](std::string(name));
}

ShaderLib::FieldProxy Material::GetField(const std::string& path) {
    return m_bufferBundle.GetField(path);
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
// GPU SYNCHRONIZATION - SYNCHRONOUS
// =============================================================================

void Material::SyncToGPU() {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }

    m_bufferBundle.SyncAllToGPU();
    SPDLOG_TRACE("Material '{}': Synced all buffers to GPU", m_name);
}

void Material::SyncFromGPU() {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }

    m_bufferBundle.SyncAllFromGPU();
    SPDLOG_TRACE("Material '{}': Synced all buffers from GPU", m_name);
}

void Material::SyncBufferToGPU(const std::string& bufferIdentifier) {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }
    m_bufferBundle.SyncBufferToGPU(bufferIdentifier);
}

void Material::SyncBufferFromGPU(const std::string& bufferIdentifier) {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }
    m_bufferBundle.SyncBufferFromGPU(bufferIdentifier);
}

void Material::SyncFieldsToGPU(const std::vector<std::string>& paths) {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }
    m_bufferBundle.SyncFieldsToGPU(paths);
}

void Material::SyncFieldsFromGPU(const std::vector<std::string>& paths) {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }
    m_bufferBundle.SyncFieldsFromGPU(paths);
}

// =============================================================================
// GPU SYNCHRONIZATION - ASYNCHRONOUS
// =============================================================================

std::vector<BufferSyncTaskHandle> Material::SyncToGPUAsync() {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }

    auto tasks = m_bufferBundle.SyncAllToGPUAsync();
    SPDLOG_TRACE("Material '{}': Started async sync to GPU ({} tasks)", m_name, tasks.size());
    return tasks;
}

std::vector<BufferSyncTaskHandle> Material::SyncFromGPUAsync() {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }

    auto tasks = m_bufferBundle.SyncAllFromGPUAsync();
    SPDLOG_TRACE("Material '{}': Started async sync from GPU ({} tasks)", m_name, tasks.size());
    return tasks;
}

BufferSyncTaskHandle Material::SyncBufferToGPUAsync(const std::string& bufferIdentifier) {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }
    return m_bufferBundle.SyncBufferToGPUAsync(bufferIdentifier);
}

BufferSyncTaskHandle Material::SyncBufferFromGPUAsync(const std::string& bufferIdentifier) {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }
    return m_bufferBundle.SyncBufferFromGPUAsync(bufferIdentifier);
}

std::vector<BufferSyncTaskHandle> Material::SyncFieldsToGPUAsync(const std::vector<std::string>& paths) {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }
    return m_bufferBundle.SyncFieldsToGPUAsync(paths);
}

std::vector<BufferSyncTaskHandle> Material::SyncFieldsFromGPUAsync(const std::vector<std::string>& paths) {
    if (!IsDescriptorSetValid()) {
        GetDescriptorSet();
    }
    return m_bufferBundle.SyncFieldsFromGPUAsync(paths);
}

// =============================================================================
// ASYNC TASK MANAGEMENT
// =============================================================================

bool Material::AreTasksComplete(const std::vector<BufferSyncTaskHandle>& tasks) const {
    return m_bufferBundle.AreTasksComplete(tasks);
}

bool Material::IsTaskComplete(BufferSyncTaskHandle task) const {
    return m_bufferBundle.AreTasksComplete({ task });
}

void Material::WaitForTasks(const std::vector<BufferSyncTaskHandle>& tasks) {
    m_bufferBundle.WaitForTasks(tasks);
    SPDLOG_TRACE("Material '{}': Waited for {} tasks", m_name, tasks.size());
}

void Material::WaitForTask(BufferSyncTaskHandle task) {
    m_bufferBundle.WaitForTasks({ task });
}

void Material::WaitForAllTasks() {
    m_bufferBundle.WaitForAllTasks();
    SPDLOG_TRACE("Material '{}': Waited for all tasks", m_name);
}

void Material::PollCompletedTasks() {
    m_bufferBundle.PollCompletedTasks();
}

size_t Material::GetActiveTaskCount() const {
    return m_bufferBundle.GetActiveTaskCount();
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

    // Bind input buffer
    if (m_inputBufferHandle.isValid()) {
        builder.bindBufferToSlot(ShaderLib::INPUT_DATA_BINDING, m_inputBufferHandle);
        auto inputBuffer = GetInputBuffer();
        if (inputBuffer) {
            inputBuffer->SetMappedBuffer(m_bufferManager.getResource(m_inputBufferHandle.handle()));
        }
        bufferCount++;
    }

    // Bind output buffer
    if (m_outputBufferHandle.isValid()) {
        builder.bindBufferToSlot(ShaderLib::OUTPUT_DATA_BINDING, m_outputBufferHandle);
        auto outputBuffer = GetOutputBuffer();
        if (outputBuffer) {
            outputBuffer->SetMappedBuffer(m_bufferManager.getResource(m_outputBufferHandle.handle()));
        }
        bufferCount++;
    }

    // Bind input/output buffer
    if (m_inputOutputBufferHandle.isValid()) {
        builder.bindBufferToSlot(ShaderLib::INPUT_OUTPUT_DATA_BINDING, m_inputOutputBufferHandle);
        auto inputOutputBuffer = GetInputOutputBuffer();
        if (inputOutputBuffer) {
            inputOutputBuffer->SetMappedBuffer(m_bufferManager.getResource(m_inputOutputBufferHandle.handle()));
        }
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

    auto inputBuffer = GetInputBuffer();
    if (inputBuffer) {
        m_inputBufferHandle = m_bufferManager.acquireSmartBuffer(
            inputBuffer->GetDefinition()
        );
    }

    auto outputBuffer = GetOutputBuffer();
    if (outputBuffer) {
        m_outputBufferHandle = m_bufferManager.acquireSmartBuffer(
            outputBuffer->GetDefinition()
        );
    }

    auto inputOutputBuffer = GetInputOutputBuffer();
    if (inputOutputBuffer) {
        m_inputOutputBufferHandle = m_bufferManager.acquireSmartBuffer(
            inputOutputBuffer->GetDefinition()
        );
    }
}

void Material::ReleaseDescriptorSetBuffers() {
    m_inputBufferHandle = SmartHandle<BufferHandle, Buffer>();
    m_outputBufferHandle = SmartHandle<BufferHandle, Buffer>();
    m_inputOutputBufferHandle = SmartHandle<BufferHandle, Buffer>();
}

// =============================================================================
// BUFFER ACCESS
// =============================================================================

std::shared_ptr<ShaderLib::BufferObjectInstance> Material::GetInputBuffer() const {
    return m_bufferBundle.GetBuffer("input");
}

std::shared_ptr<ShaderLib::BufferObjectInstance> Material::GetOutputBuffer() const {
    return m_bufferBundle.GetBuffer("output");
}

std::shared_ptr<ShaderLib::BufferObjectInstance> Material::GetInputOutputBuffer() const {
    return m_bufferBundle.GetBuffer("inputOutput");
}

bool Material::HasInputBuffer() const {
    return m_bufferBundle.HasBuffer("input");
}

bool Material::HasOutputBuffer() const {
    return m_bufferBundle.HasBuffer("output");
}

bool Material::HasInputOutputBuffer() const {
    return m_bufferBundle.HasBuffer("inputOutput");
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
