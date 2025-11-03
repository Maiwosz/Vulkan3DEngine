#include "DescriptorSetBuilder.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

DescriptorSetBuilder::DescriptorSetBuilder(
    const LogicalDevice& device,
    BufferManager& bufferManager,
    ImageSamplerManager& samplerManager,
    TextureManager& textureManager,
    DescriptorAllocator& descriptorAllocator,
    DescriptorLayoutManager& descriptorLayoutManager
)
    : m_device(device),
    m_bufferManager(bufferManager),
    m_samplerManager(samplerManager),
    m_textureManager(textureManager),
    m_descriptorAllocator(descriptorAllocator),
    m_descriptorLayoutManager(descriptorLayoutManager) {
}

DescriptorSetBuilder& DescriptorSetBuilder::forDescriptorSet(
    const ShaderLib::DescriptorSet& descriptorSet,
    DescriptorLayoutHandle layoutHandle
) {
    clear();
    m_descriptorSetMetadata = &descriptorSet;
    m_layoutHandle = layoutHandle;

    SPDLOG_DEBUG("Building descriptor set {} with {} slots and {} buffers",
        descriptorSet.setNumber, descriptorSet.slots.size(), descriptorSet.buffers.size());

    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bindBuffer(const std::string& name, SmartHandle<BufferHandle, Buffer> buffer) {
    if (!m_descriptorSetMetadata) {
        SPDLOG_ERROR("Cannot bind buffer '{}': no descriptor set metadata set", name);
        return *this;
    }

    const ShaderLib::DescriptorSlot* slot = findSlotByName(name);
    if (!slot) {
        SPDLOG_ERROR("Cannot bind buffer '{}': slot not found in descriptor set {}",
            name, m_descriptorSetMetadata->setNumber);
        return *this;
    }

    if (!slot->IsBuffer()) {
        SPDLOG_ERROR("Cannot bind buffer to '{}': slot {} is not a buffer type",
            name, slot->binding);
        return *this;
    }

    PendingBinding pending;
    pending.binding = slot->binding;
    pending.value = BufferBinding{ std::move(buffer) };
    pending.slot = slot;

    m_pendingBindings.push_back(std::move(pending));

    SPDLOG_TRACE("Queued buffer binding for '{}' at binding {}", name, slot->binding);
    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bindTexture(const std::string& name, TextureHandle texture, SamplerHandle sampler) {
    if (!m_descriptorSetMetadata) {
        SPDLOG_ERROR("Cannot bind texture '{}': no descriptor set metadata set", name);
        return *this;
    }

    const ShaderLib::DescriptorSlot* slot = findSlotByName(name);
    if (!slot) {
        SPDLOG_ERROR("Cannot bind texture '{}': slot not found in descriptor set {}",
            name, m_descriptorSetMetadata->setNumber);
        return *this;
    }

    if (!slot->IsSampler()) {
        SPDLOG_ERROR("Cannot bind texture to '{}': slot {} is not a sampler type",
            name, slot->binding);
        return *this;
    }

    PendingBinding pending;
    pending.binding = slot->binding;
    pending.value = TextureBinding{ texture, sampler };
    pending.slot = slot;

    m_pendingBindings.push_back(std::move(pending));

    SPDLOG_TRACE("Queued texture binding for '{}' at binding {}", name, slot->binding);
    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bindImageView(const std::string& name, VkImageView imageView, SamplerHandle sampler) {
    if (!m_descriptorSetMetadata) {
        SPDLOG_ERROR("Cannot bind image view '{}': no descriptor set metadata set", name);
        return *this;
    }

    const ShaderLib::DescriptorSlot* slot = findSlotByName(name);
    if (!slot) {
        SPDLOG_ERROR("Cannot bind image view '{}': slot not found in descriptor set {}",
            name, m_descriptorSetMetadata->setNumber);
        return *this;
    }

    if (!slot->IsSampler()) {
        SPDLOG_ERROR("Cannot bind image view to '{}': slot {} is not a sampler type",
            name, slot->binding);
        return *this;
    }

    PendingBinding pending;
    pending.binding = slot->binding;
    pending.value = ImageBinding{ imageView, sampler };
    pending.slot = slot;

    m_pendingBindings.push_back(std::move(pending));

    SPDLOG_TRACE("Queued image view binding for '{}' at binding {}", name, slot->binding);
    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bindBufferToSlot(uint32_t binding, SmartHandle<BufferHandle, Buffer> buffer) {
    if (!m_descriptorSetMetadata) {
        SPDLOG_ERROR("Cannot bind buffer to slot {}: no descriptor set metadata set", binding);
        return *this;
    }

    const ShaderLib::DescriptorSlot* slot = m_descriptorSetMetadata->FindSlot(binding);
    if (!slot) {
        SPDLOG_ERROR("Cannot bind buffer: slot {} not found in descriptor set {}",
            binding, m_descriptorSetMetadata->setNumber);
        return *this;
    }

    if (!slot->IsBuffer()) {
        SPDLOG_ERROR("Cannot bind buffer to slot {}: not a buffer type", binding);
        return *this;
    }

    PendingBinding pending;
    pending.binding = binding;
    pending.value = BufferBinding{ std::move(buffer) };
    pending.slot = slot;

    m_pendingBindings.push_back(std::move(pending));

    SPDLOG_TRACE("Queued buffer binding at slot {}", binding);
    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bindTextureToSlot(uint32_t binding, TextureHandle texture, SamplerHandle sampler) {
    if (!m_descriptorSetMetadata) {
        SPDLOG_ERROR("Cannot bind texture to slot {}: no descriptor set metadata set", binding);
        return *this;
    }

    const ShaderLib::DescriptorSlot* slot = m_descriptorSetMetadata->FindSlot(binding);
    if (!slot) {
        SPDLOG_ERROR("Cannot bind texture: slot {} not found in descriptor set {}",
            binding, m_descriptorSetMetadata->setNumber);
        return *this;
    }

    if (!slot->IsSampler()) {
        SPDLOG_ERROR("Cannot bind texture to slot {}: not a sampler type", binding);
        return *this;
    }

    PendingBinding pending;
    pending.binding = binding;
    pending.value = TextureBinding{ texture, sampler };
    pending.slot = slot;

    m_pendingBindings.push_back(std::move(pending));

    SPDLOG_TRACE("Queued texture binding at slot {}", binding);
    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bindImageViewToSlot(uint32_t binding, VkImageView imageView, SamplerHandle sampler) {
    if (!m_descriptorSetMetadata) {
        SPDLOG_ERROR("Cannot bind image view to slot {}: no descriptor set metadata set", binding);
        return *this;
    }

    const ShaderLib::DescriptorSlot* slot = m_descriptorSetMetadata->FindSlot(binding);
    if (!slot) {
        SPDLOG_ERROR("Cannot bind image view: slot {} not found in descriptor set {}",
            binding, m_descriptorSetMetadata->setNumber);
        return *this;
    }

    if (!slot->IsSampler()) {
        SPDLOG_ERROR("Cannot bind image view to slot {}: not a sampler type", binding);
        return *this;
    }

    PendingBinding pending;
    pending.binding = binding;
    pending.value = ImageBinding{ imageView, sampler };
    pending.slot = slot;

    m_pendingBindings.push_back(std::move(pending));

    SPDLOG_TRACE("Queued image view binding at slot {}", binding);
    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bindBufferVariables(
    const std::string& bufferName,
    const std::unordered_map<std::string, ShaderLib::BufferValue>& variables
) {
    if (!m_descriptorSetMetadata) {
        SPDLOG_ERROR("Cannot bind buffer variables for '{}': no descriptor set metadata set", bufferName);
        return *this;
    }

    // Find the buffer in metadata
    const ShaderLib::BufferObject* bufferObj = findBufferByName(bufferName);
    if (!bufferObj) {
        SPDLOG_ERROR("Buffer '{}' not found in descriptor set {}",
            bufferName, m_descriptorSetMetadata->setNumber);
        return *this;
    }

    // Check if we already created a buffer for this name
    auto it = m_createdBuffers.find(bufferName);
    SmartHandle<BufferHandle, Buffer> bufferHandle;

    if (it != m_createdBuffers.end()) {
        bufferHandle = it->second;
    }
    else {
        // Create new buffer from BufferObject metadata
        bufferHandle = m_bufferManager.acquireSmartBuffer(*bufferObj);
        m_createdBuffers[bufferName] = bufferHandle;
    }

    // Update buffer with variables
    auto mappedWriter = m_bufferManager.createWriter(bufferHandle.handle());
    if (!mappedWriter.isValid()) {
        SPDLOG_ERROR("Failed to map buffer '{}' for writing", bufferName);
        return *this;
    }

    uint32_t updatedCount = 0;
    for (const auto& [varName, value] : variables) {
        if (mappedWriter.write(varName, value)) {
            updatedCount++;
            SPDLOG_TRACE("Updated variable '{}' in buffer '{}'", varName, bufferName);
        }
        else {
            SPDLOG_WARN("Failed to write variable '{}' to buffer '{}'", varName, bufferName);
        }
    }

    SPDLOG_DEBUG("Updated {} variables in buffer '{}'", updatedCount, bufferName);

    // Find the slot for this buffer and add to pending bindings
    const ShaderLib::DescriptorSlot* slot = findSlotByName(bufferName);
    if (!slot) {
        SPDLOG_ERROR("Cannot find descriptor slot for buffer '{}'", bufferName);
        return *this;
    }

    PendingBinding pending;
    pending.binding = slot->binding;
    pending.value = BufferBinding{ bufferHandle };
    pending.slot = slot;

    m_pendingBindings.push_back(std::move(pending));

    return *this;
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> DescriptorSetBuilder::build() {
    if (!m_descriptorSetMetadata) {
        SPDLOG_ERROR("Cannot build descriptor set: no metadata set");
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    if (!validateBindings()) {
        auto missing = getMissingBindings();
        SPDLOG_ERROR("Cannot build descriptor set {}: missing {} required bindings",
            m_descriptorSetMetadata->setNumber, missing.size());
        for (const auto& name : missing) {
            SPDLOG_ERROR("  - Missing binding: {}", name);
        }
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    // Get descriptor set layout
    VkDescriptorSetLayout layout = getLayout();
    if (layout == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Cannot build descriptor set: invalid layout");
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    // Create low-level writer
    DescriptorWriter writer(m_device, m_samplerManager, m_bufferManager, m_descriptorAllocator);

    // Write all pending bindings
    writeBindingsToWriter(writer);

    // Build and return descriptor set
    auto descriptorSet = writer.createDescriptorSet(layout);

    SPDLOG_INFO("Built descriptor set {} with {} bindings",
        m_descriptorSetMetadata->setNumber, m_pendingBindings.size());

    // Clear builder state for reuse
    m_pendingBindings.clear();

    return descriptorSet;
}

void DescriptorSetBuilder::clear() {
    m_descriptorSetMetadata = nullptr;
    m_layoutHandle = DescriptorLayoutHandle{};
    m_pendingBindings.clear();
    m_createdBuffers.clear();
}

bool DescriptorSetBuilder::validateBindings() const {
    if (!m_descriptorSetMetadata) {
        return false;
    }

    // Create set of bound binding numbers
    std::unordered_set<uint32_t> boundBindings;
    for (const auto& pending : m_pendingBindings) {
        boundBindings.insert(pending.binding);
    }

    // Check all required slots are bound
    for (const auto& slot : m_descriptorSetMetadata->slots) {
        if (boundBindings.find(slot.binding) == boundBindings.end()) {
            SPDLOG_WARN("Binding {} ('{}') not bound in descriptor set {}",
                slot.binding, slot.name, m_descriptorSetMetadata->setNumber);
            return false;
        }
    }

    return true;
}

std::vector<std::string> DescriptorSetBuilder::getMissingBindings() const {
    std::vector<std::string> missing;

    if (!m_descriptorSetMetadata) {
        return missing;
    }

    // Create set of bound binding numbers
    std::unordered_set<uint32_t> boundBindings;
    for (const auto& pending : m_pendingBindings) {
        boundBindings.insert(pending.binding);
    }

    // Check all required slots
    for (const auto& slot : m_descriptorSetMetadata->slots) {
        if (boundBindings.find(slot.binding) == boundBindings.end()) {
            missing.push_back(slot.name);
        }
    }

    return missing;
}

const ShaderLib::DescriptorSlot* DescriptorSetBuilder::findSlotByName(const std::string& name) const {
    if (!m_descriptorSetMetadata) {
        return nullptr;
    }
    return m_descriptorSetMetadata->FindSlot(name);
}

const ShaderLib::BufferObject* DescriptorSetBuilder::findBufferByName(const std::string& name) const {
    if (!m_descriptorSetMetadata) {
        return nullptr;
    }
    return m_descriptorSetMetadata->GetBuffer(name);
}

void DescriptorSetBuilder::writeBindingsToWriter(DescriptorWriter& writer) {
    for (const auto& pending : m_pendingBindings) {
        if (auto* bufferBinding = std::get_if<BufferBinding>(&pending.value)) {
            writeBufferBinding(writer, pending.binding, *bufferBinding);
        }
        else if (auto* textureBinding = std::get_if<TextureBinding>(&pending.value)) {
            writeTextureBinding(writer, pending.binding, *textureBinding);
        }
        else if (auto* imageBinding = std::get_if<ImageBinding>(&pending.value)) {
            writeImageBinding(writer, pending.binding, *imageBinding);
        }
    }
}

void DescriptorSetBuilder::writeBufferBinding(DescriptorWriter& writer, uint32_t binding, const BufferBinding& bufferBinding) {
    if (!bufferBinding.buffer.isValid()) {
        SPDLOG_WARN("Skipping invalid buffer binding at slot {}", binding);
        return;
    }

    writer.writeBuffer(binding, bufferBinding.buffer);
    SPDLOG_TRACE("Wrote buffer binding at slot {}", binding);
}

void DescriptorSetBuilder::writeTextureBinding(DescriptorWriter& writer, uint32_t binding, const TextureBinding& textureBinding) {
    if (!textureBinding.texture.isValid()) {
        SPDLOG_WARN("Skipping invalid texture binding at slot {}", binding);
        return;
    }

    VkImageView imageView = m_textureManager.getImageView(textureBinding.texture);
    if (imageView == VK_NULL_HANDLE) {
        SPDLOG_WARN("Failed to get image view for texture at slot {}", binding);
        return;
    }

    writer.writeCombinedImageSampler(binding, imageView, textureBinding.sampler);
    SPDLOG_TRACE("Wrote texture binding at slot {}", binding);
}

void DescriptorSetBuilder::writeImageBinding(DescriptorWriter& writer, uint32_t binding, const ImageBinding& imageBinding) {
    if (imageBinding.imageView == VK_NULL_HANDLE) {
        SPDLOG_WARN("Skipping invalid image view binding at slot {}", binding);
        return;
    }

    writer.writeCombinedImageSampler(binding, imageBinding.imageView, imageBinding.sampler);
    SPDLOG_TRACE("Wrote image view binding at slot {}", binding);
}

VkDescriptorSetLayout DescriptorSetBuilder::getLayout() const {
    if (!m_descriptorSetMetadata || !m_layoutHandle) {
        return VK_NULL_HANDLE;
    }

    return m_descriptorLayoutManager.get(m_layoutHandle);
}