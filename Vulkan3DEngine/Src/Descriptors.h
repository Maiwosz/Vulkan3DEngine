#pragma once
#include "Prerequisites.h"
#include <deque>
#include <span>


class DescriptorLayoutBuilder {
public:
    void addBinding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
private:
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

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

class DescriptorAllocator {
public:
    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };
    void initPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
    void clearDescriptors(VkDevice device);
    void destroyPool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
private:
    VkDescriptorPool pool;
};

class DescriptorAllocatorGrowable {
public:
    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
    void clearPools(VkDevice device);
    void destroyPools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);
    VkDescriptorPool getPool(VkDevice device);
    VkDescriptorPool createPool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

    std::vector<PoolSizeRatio> ratios;
private:
    std::vector<VkDescriptorPool> fullPools;
    std::vector<VkDescriptorPool> readyPools;
    uint32_t setsPerPool;
};
