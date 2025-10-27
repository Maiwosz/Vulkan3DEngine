#pragma once
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>
#include "LogicalDevice.h"
#include "BufferManager.h"
#include "ImageSamplerManager.h"
#include "DescriptorAllocator.h"
#include "ISmartHandleManager.h"
#include "Handle.h"

class DescriptorWriter {
public:
    DescriptorWriter(const LogicalDevice& device,
        ImageSamplerManager& samplerManager,
        BufferManager& uniformBufferManager,
        DescriptorAllocator& descriptorAllocator);

    // Write uniform buffer to binding
    void writeUniformBuffer(int binding, SmartHandle<BufferHandle, Buffer> uniformBuffer);

    // Write combined image sampler to binding
    void writeCombinedImageSampler(int binding, VkImageView imageView, SamplerHandle sampler);

    // Write separate texture and sampler
    void writeTexture(int binding, VkImageView imageView);
    void writeSampler(int binding, SamplerHandle sampler);

    // Clear all pending writes
    void clear();

    // Create descriptor set with current bindings and return smart handle
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> createDescriptorSet(VkDescriptorSetLayout layout);

private:
    struct UniformBufferBinding {
        int binding;
        SmartHandle<BufferHandle, Buffer> uniformBuffer;
    };

    struct ImageBinding {
        int binding;
        VkImageView imageView;
        SamplerHandle sampler;
        VkDescriptorType type;
    };

    struct SamplerBinding {
        int binding;
        SamplerHandle sampler;
    };

    const LogicalDevice& m_device;
    ImageSamplerManager& m_samplerManager;
    BufferManager& m_bufferManager;
    DescriptorAllocator& m_descriptorAllocator;

    std::vector<UniformBufferBinding> m_uniformBufferBindings;
    std::vector<ImageBinding> m_imageBindings;
    std::vector<SamplerBinding> m_samplerBindings;
};