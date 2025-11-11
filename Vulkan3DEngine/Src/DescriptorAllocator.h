#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <queue>
#include <vulkan/vulkan.h>
#include "LogicalDevice.h"
#include "Handle.h"
#include "ISmartHandleManager.h"
#include "BufferManager.h"
#include "ImageSamplerManager.h"

// Forward declarations
class Buffer;
class ImageSampler;

/**
 * Simplified DescriptorAllocator using DescriptorSetGuards for GPU usage tracking.
 * No longer needs frame-based tracking - guards handle it automatically.
 */
class DescriptorAllocator : public ISmartHandleManager<DescriptorSetHandle, VkDescriptorSet> {
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

    // Structure to hold resources bound to a descriptor set
    struct DescriptorResources {
        std::vector<SmartHandle<BufferHandle, Buffer>> uniformBuffers;
        std::vector<SamplerHandle> samplers;

        void clear() {
            uniformBuffers.clear();
            samplers.clear();
        }
    };

    DescriptorAllocator(const LogicalDevice& device, const PoolConfig& config);
    ~DescriptorAllocator();
    void reset();
    void destroy();

    // Main interface - simplified (no frame tracking needed)
    DescriptorSetHandle acquireDescriptorSet(VkDescriptorSetLayout layout);
    DescriptorSetHandle acquireDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources);

    void releaseDescriptorSet(DescriptorSetHandle handle);
    VkDescriptorSet getDescriptorSet(DescriptorSetHandle handle) const;

    // Resource management
    void bindUniformBuffer(DescriptorSetHandle handle, SmartHandle<BufferHandle, Buffer> buffer);
    void bindSampler(DescriptorSetHandle handle, SamplerHandle sampler);
    void bindResources(DescriptorSetHandle handle, const DescriptorResources& resources);

    // Get resources bound to a descriptor set
    const DescriptorResources& getDescriptorResources(DescriptorSetHandle handle) const;
    DescriptorResources& getDescriptorResources(DescriptorSetHandle handle);

    // Smart handle support
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> acquireSmartDescriptorSet(VkDescriptorSetLayout layout);
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> acquireSmartDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources);

    // IResourceManager interface implementation
    VkDescriptorSet* getResource(DescriptorSetHandle handle) override;
    bool isValid(DescriptorSetHandle handle) const override;
    void releaseResource(DescriptorSetHandle handle) override;
    void addReference(DescriptorSetHandle handle) override;
    void removeReference(DescriptorSetHandle handle) override;

    // Statistics for debugging
    struct Stats {
        size_t totalAllocated = 0;
        size_t inUse = 0;
        size_t reusable = 0;
        size_t poolCount = 0;
    };
    Stats getStats() const;

private:
    struct DescriptorSetEntry {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VkDescriptorPool sourcePool = VK_NULL_HANDLE;
        bool inUse = false;           // Currently used by application
        bool isAllocated = false;     // Has been allocated from pool
        uint32_t referenceCount = 0;
        DescriptorResources resources;
    };

    // Pool management methods
    VkResult createPool(uint32_t setCount, VkDescriptorPool* outPool) const;
    VkDescriptorPool getPool();
    uint32_t computeMinSetCount() const;

    // Handle management methods
    DescriptorSetHandle createNewDescriptorSet(VkDescriptorSetLayout layout);
    DescriptorSetHandle createNewDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources);
    DescriptorSetHandle findReusableDescriptorSet(VkDescriptorSetLayout layout);

    // Pool management
    std::vector<VkDescriptorPool> m_fullPools;
    std::vector<VkDescriptorPool> m_readyPools;
    PoolConfig m_config;
    uint32_t m_nextSetCount;
    const LogicalDevice& m_device;

    // Handle management
    std::vector<DescriptorSetEntry> m_descriptorSets;
    std::unordered_map<VkDescriptorSetLayout, std::queue<DescriptorSetHandle>> m_reusableSets;
    uint32_t m_nextHandleId;

    // Cache for getResource (to return pointer)
    mutable std::unordered_map<DescriptorSetHandle, VkDescriptorSet> m_resourceCache;

    // Empty resources for invalid handles
    static const DescriptorResources s_emptyResources;
};
