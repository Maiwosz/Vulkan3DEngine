#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <queue>
#include <vulkan/vulkan.h>
#include "LogicalDevice.h"
#include "Handle.h"
#include "ISmartHandleManager.h"
#include "UniformBufferManager.h"
#include "ImageSamplerManager.h"

// Forward declarations
class Buffer;
class ImageSampler;

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
        std::vector<SmartHandle<UniformBufferHandle, Buffer>> uniformBuffers;
        std::vector<SamplerHandle> samplers; // Regular handles since ImageSamplerManager doesn't support smart handles

        void clear() {
            uniformBuffers.clear();
            samplers.clear();
        }
    };

    DescriptorAllocator(const LogicalDevice& device, const PoolConfig& config);
    ~DescriptorAllocator();
    void reset();
    void destroy();

    // Enhanced interface with resource tracking
    DescriptorSetHandle acquireDescriptorSet(VkDescriptorSetLayout layout);
    DescriptorSetHandle acquireDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources);

    void releaseDescriptorSet(DescriptorSetHandle handle);
    VkDescriptorSet getDescriptorSet(DescriptorSetHandle handle) const;

    // Resource management
    void bindUniformBuffer(DescriptorSetHandle handle, SmartHandle<UniformBufferHandle, Buffer> buffer);
    void bindSampler(DescriptorSetHandle handle, SamplerHandle sampler);
    void bindResources(DescriptorSetHandle handle, const DescriptorResources& resources);

    // Get resources bound to a descriptor set
    const DescriptorResources& getDescriptorResources(DescriptorSetHandle handle) const;
    DescriptorResources& getDescriptorResources(DescriptorSetHandle handle);

    // Smart handle support - publiczny factory method
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> acquireSmartDescriptorSet(VkDescriptorSetLayout layout);
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> acquireSmartDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources);

    // IResourceManager interface implementation
    VkDescriptorSet* getResource(DescriptorSetHandle handle) override;
    bool isValid(DescriptorSetHandle handle) const override;
    void releaseResource(DescriptorSetHandle handle) override;
    void addReference(DescriptorSetHandle handle) override;
    void removeReference(DescriptorSetHandle handle) override;

private:
    struct DescriptorSetEntry {
        VkDescriptorSet descriptorSet;
        VkDescriptorSetLayout layout;
        VkDescriptorPool sourcePool;
        bool inUse;
        uint32_t referenceCount;
        DescriptorResources resources; // Resources bound to this descriptor set
    };

    // Prywatne metody zarządzania pulami
    VkResult createPool(uint32_t setCount, VkDescriptorPool* outPool) const;
    VkDescriptorPool getPool();
    uint32_t computeMinSetCount() const;

    // Prywatne metody zarządzania uchwytami
    DescriptorSetHandle createNewDescriptorSet(VkDescriptorSetLayout layout);
    DescriptorSetHandle createNewDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources);
    DescriptorSetHandle findReusableDescriptorSet(VkDescriptorSetLayout layout);

    std::vector<VkDescriptorPool> m_fullPools;
    std::vector<VkDescriptorPool> m_readyPools;
    PoolConfig m_config;
    uint32_t m_nextSetCount;
    const LogicalDevice& m_device;

    // Struktury dla zarządzania uchwytami
    std::vector<DescriptorSetEntry> m_descriptorSets;
    std::unordered_map<VkDescriptorSetLayout, std::queue<DescriptorSetHandle>> m_reusableSets;
    uint32_t m_nextHandleId;

    // Cache dla getResource (żeby zwrócić wskaźnik)
    mutable std::unordered_map<DescriptorSetHandle, VkDescriptorSet> m_resourceCache;

    // Empty resources for invalid handles
    static const DescriptorResources s_emptyResources;
};