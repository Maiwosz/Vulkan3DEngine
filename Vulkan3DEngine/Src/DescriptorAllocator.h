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

    // GPU usage tracking - to be called by RenderSystem
    void markDescriptorAsUsedByGPU(DescriptorSetHandle handle, uint32_t frameIndex);
    void markFrameCompleted(uint32_t frameIndex);

    // Enhanced interface with resource tracking
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

private:
    struct DescriptorSetEntry {
        VkDescriptorSet descriptorSet;
        VkDescriptorSetLayout layout;
        VkDescriptorPool sourcePool;
        bool inUse;                    // Czy jest aktywnie używany przez aplikację
        bool isAllocated;              // Czy został przydzielony z poola
        bool usedByGPU;                // Czy jest używany przez GPU
        uint32_t gpuFrameIndex;        // W której klatce jest używany przez GPU
        uint32_t referenceCount;
        DescriptorResources resources;
    };

    // GPU usage tracking structures
    struct FrameGpuUsage {
        std::vector<DescriptorSetHandle> usedDescriptors;
        bool completed = false;
    };

    // Pool management methods
    VkResult createPool(uint32_t setCount, VkDescriptorPool* outPool) const;
    VkDescriptorPool getPool();
    uint32_t computeMinSetCount() const;

    // Handle management methods
    DescriptorSetHandle createNewDescriptorSet(VkDescriptorSetLayout layout);
    DescriptorSetHandle createNewDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources);
    DescriptorSetHandle findReusableDescriptorSet(VkDescriptorSetLayout layout);

    // Internal GPU usage tracking
    void releaseGpuUsageForFrame(uint32_t frameIndex);

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

    // GPU usage tracking
    std::unordered_map<uint32_t, FrameGpuUsage> m_frameGpuUsage;

    // Cache for getResource (to return pointer)
    mutable std::unordered_map<DescriptorSetHandle, VkDescriptorSet> m_resourceCache;

    // Empty resources for invalid handles
    static const DescriptorResources s_emptyResources;
};