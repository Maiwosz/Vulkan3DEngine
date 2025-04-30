#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <vulkan/vulkan.h>

class DescriptorWriter {
public:
    void writeImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
    void writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);
    void clear();
    void updateSet(VkDevice device, VkDescriptorSet set);
private:
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;
};