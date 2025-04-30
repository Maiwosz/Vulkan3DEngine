#pragma once
#include <cstdint>
#include <vector>
#include <cmath>
#include <vulkan/vulkan.h>
#include "LogicalDevice.h"

class DescriptorAllocator {
public:
    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    struct PoolConfig {
        uint32_t initialSets = 512;
        std::vector<PoolSizeRatio> ratios;
        float growthFactor = 1.5f;
    };

    DescriptorAllocator(LogicalDevice& device, const PoolConfig& config);
    void reset();
    void destroy();

    VkDescriptorSet allocate(VkDescriptorSetLayout layout);

private:
    VkResult createPool(uint32_t setCount, VkDescriptorPool* outPool) const;
    VkDescriptorPool getPool();
    uint32_t computeMinSetCount() const;

    std::vector<VkDescriptorPool> m_fullPools;
    std::vector<VkDescriptorPool> m_readyPools;
    PoolConfig m_config;
    uint32_t m_nextSetCount;
    LogicalDevice& m_device;
};