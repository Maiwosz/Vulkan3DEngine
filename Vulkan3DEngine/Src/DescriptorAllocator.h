#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <queue>
#include <vulkan/vulkan.h>
#include "LogicalDevice.h"
#include "Handle.h"

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

    DescriptorAllocator(const LogicalDevice& device, const PoolConfig& config);
    void reset();
    void destroy();

    // Nowy publiczny interfejs
    DescriptorSetHandle acquireDescriptorSet(VkDescriptorSetLayout layout);
    void releaseDescriptorSet(DescriptorSetHandle handle);
    VkDescriptorSet getDescriptorSet(DescriptorSetHandle handle) const;

private:
    struct DescriptorSetEntry {
        VkDescriptorSet descriptorSet;
        VkDescriptorSetLayout layout;
        VkDescriptorPool sourcePool;
        bool inUse;
    };

    // Prywatne metody
    VkResult createPool(uint32_t setCount, VkDescriptorPool* outPool) const;
    VkDescriptorPool getPool();
    uint32_t computeMinSetCount() const;

    DescriptorSetHandle createNewDescriptorSet(VkDescriptorSetLayout layout);
    DescriptorSetHandle findReusableDescriptorSet(VkDescriptorSetLayout layout);

    std::vector<VkDescriptorPool> m_fullPools;
    std::vector<VkDescriptorPool> m_readyPools;
    PoolConfig m_config;
    uint32_t m_nextSetCount;
    const LogicalDevice& m_device;

    // Nowe struktury dla zarządzania uchwytami
    std::vector<DescriptorSetEntry> m_descriptorSets;
    std::queue<DescriptorSetHandle> m_freeHandles;
    std::unordered_map<VkDescriptorSetLayout, std::queue<DescriptorSetHandle>> m_reusableSets;
    uint32_t m_nextHandleId;
};