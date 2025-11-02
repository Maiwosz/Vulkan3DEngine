#include "DescriptorWriter.h"
#include <deque>

DescriptorWriter::DescriptorWriter(const LogicalDevice& device,
    ImageSamplerManager& samplerManager,
    BufferManager& uniformBufferManager,
    DescriptorAllocator& descriptorAllocator)
    : m_device(device)
    , m_samplerManager(samplerManager)
    , m_bufferManager(uniformBufferManager)
    , m_descriptorAllocator(descriptorAllocator)
{
}

void DescriptorWriter::writeBuffer(int binding, SmartHandle<BufferHandle, Buffer> buffer)
{
    // Get buffer info to determine type
    const BufferInfo& bufferInfo = m_bufferManager.getBufferInfo(buffer.handle());

    VkDescriptorType vkDescriptorType;
    if (bufferInfo.bufferType == ShaderLib::BufferType::Uniform) {
        vkDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    else if (bufferInfo.bufferType == ShaderLib::BufferType::Storage) {
        vkDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    else {
        // Fallback: shouldn't happen
        vkDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }

    m_bufferBindings.push_back({
        .binding = binding,
        .buffer = std::move(buffer),
        .descriptorType = vkDescriptorType
        });
}

void DescriptorWriter::writeCombinedImageSampler(int binding, VkImageView imageView, SamplerHandle sampler)
{
    m_imageBindings.push_back({
        .binding = binding,
        .imageView = imageView,
        .sampler = sampler,
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        });
}

void DescriptorWriter::writeTexture(int binding, VkImageView imageView)
{
    m_imageBindings.push_back({
        .binding = binding,
        .imageView = imageView,
        .sampler = SamplerHandle{}, // Invalid handle for texture-only binding
        .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
        });
}

void DescriptorWriter::writeSampler(int binding, SamplerHandle sampler)
{
    m_samplerBindings.push_back({
        .binding = binding,
        .sampler = sampler
        });
}

void DescriptorWriter::clear()
{
    m_bufferBindings.clear();
    m_imageBindings.clear();
    m_samplerBindings.clear();
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> DescriptorWriter::createDescriptorSet(VkDescriptorSetLayout layout)
{
    // Prepare resources that will be bound to the descriptor set
    DescriptorAllocator::DescriptorResources resources;

    // Collect all buffer smart handles (both UBO and SSBO)
    for (const auto& binding : m_bufferBindings) {
        resources.uniformBuffers.push_back(binding.buffer);
    }

    // Collect all sampler handles (from both image bindings and separate sampler bindings)
    for (const auto& binding : m_imageBindings) {
        if (binding.sampler.isValid()) {
            resources.samplers.push_back(binding.sampler);
        }
    }
    for (const auto& binding : m_samplerBindings) {
        resources.samplers.push_back(binding.sampler);
    }

    // Create descriptor set with bound resources
    auto smartDescriptorSet = m_descriptorAllocator.acquireSmartDescriptorSet(layout, resources);

    if (!smartDescriptorSet) {
        return smartDescriptorSet; // Return invalid handle on failure
    }

    // Now write the actual descriptor data
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    // Write buffers (both UBO and SSBO with correct descriptor types)
    for (const auto& binding : m_bufferBindings) {
        if (binding.buffer && binding.buffer.get()) {
            VkDescriptorBufferInfo& bufferInfo = bufferInfos.emplace_back(VkDescriptorBufferInfo{
                .buffer = binding.buffer.get()->get(),
                .offset = 0,
                .range = VK_WHOLE_SIZE
                });

            VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = *smartDescriptorSet.get(),
                .dstBinding = static_cast<uint32_t>(binding.binding),
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = binding.descriptorType,  // Use the correct type!
                .pBufferInfo = &bufferInfo
            };

            writes.push_back(write);
        }
    }

    // Write images (combined image samplers or separate textures)
    for (const auto& binding : m_imageBindings) {
        VkSampler samplerVk = VK_NULL_HANDLE;
        if (binding.sampler.isValid()) {
            ImageSampler* samplerResource = m_samplerManager.getResource(binding.sampler);
            if (samplerResource) {
                samplerVk = samplerResource->get();
            }
        }

        VkDescriptorImageInfo& imageInfo = imageInfos.emplace_back(VkDescriptorImageInfo{
            .sampler = samplerVk,
            .imageView = binding.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = *smartDescriptorSet.get(),
            .dstBinding = static_cast<uint32_t>(binding.binding),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = binding.type,
            .pImageInfo = &imageInfo
        };

        writes.push_back(write);
    }

    // Write separate samplers
    for (const auto& binding : m_samplerBindings) {
        if (binding.sampler.isValid()) {
            ImageSampler* samplerResource = m_samplerManager.getResource(binding.sampler);
            if (samplerResource) {
                VkDescriptorImageInfo& imageInfo = imageInfos.emplace_back(VkDescriptorImageInfo{
                    .sampler = samplerResource->get(),
                    .imageView = VK_NULL_HANDLE,
                    .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
                    });

                VkWriteDescriptorSet write = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = *smartDescriptorSet.get(),
                    .dstBinding = static_cast<uint32_t>(binding.binding),
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .pImageInfo = &imageInfo
                };

                writes.push_back(write);
            }
        }
    }

    // Update descriptor set
    if (!writes.empty()) {
        vkUpdateDescriptorSets(m_device.get(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    return smartDescriptorSet;
}