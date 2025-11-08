#include "DescriptorAllocator.h"
#include "Prerequisites.h"

// Static member definition
const DescriptorAllocator::DescriptorResources DescriptorAllocator::s_emptyResources;

DescriptorAllocator::DescriptorAllocator(const LogicalDevice& device, const PoolConfig& config) :
    m_device(device), m_config(config), m_nextSetCount(config.initialSets), m_nextHandleId(1)
{
    // Initialize with first pool
    m_readyPools.push_back(getPool());
}

DescriptorAllocator::~DescriptorAllocator() {
    destroy();
}

// GPU usage tracking methods
void DescriptorAllocator::markDescriptorAsUsedByGPU(DescriptorSetHandle handle, uint32_t frameIndex) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) {
        return;
    }

    auto& entry = m_descriptorSets[handle.id - 1];
    if (!entry.inUse || !entry.isAllocated) {
        return;
    }

    // Mark as used by GPU
    entry.usedByGPU = true;
    entry.gpuFrameIndex = frameIndex;

    // Add to frame tracking
    auto& frameUsage = m_frameGpuUsage[frameIndex];
    frameUsage.usedDescriptors.push_back(handle);
    frameUsage.completed = false;
}

void DescriptorAllocator::markFrameCompleted(uint32_t frameIndex) {
    auto it = m_frameGpuUsage.find(frameIndex);
    if (it != m_frameGpuUsage.end()) {
        it->second.completed = true;
        releaseGpuUsageForFrame(frameIndex);
    }
}

void DescriptorAllocator::releaseGpuUsageForFrame(uint32_t frameIndex) {
    auto it = m_frameGpuUsage.find(frameIndex);
    if (it == m_frameGpuUsage.end() || !it->second.completed) {
        return;
    }

    // Release GPU usage flag for all descriptors in this frame
    for (DescriptorSetHandle handle : it->second.usedDescriptors) {
        if (handle.isValid() && handle.id <= m_descriptorSets.size()) {
            auto& entry = m_descriptorSets[handle.id - 1];
            if (entry.gpuFrameIndex == frameIndex) {
                entry.usedByGPU = false;
                entry.gpuFrameIndex = 0;

                // If not in use by application, make it reusable
                if (!entry.inUse && entry.referenceCount == 0) {
                    m_reusableSets[entry.layout].push(handle);
                }
            }
        }
    }

    // Remove frame tracking data
    m_frameGpuUsage.erase(it);
}

// Enhanced public interface
DescriptorSetHandle DescriptorAllocator::acquireDescriptorSet(VkDescriptorSetLayout layout) {
    return acquireDescriptorSet(layout, DescriptorResources{});
}

DescriptorSetHandle DescriptorAllocator::acquireDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources) {
    // Try to find reusable descriptor set
    DescriptorSetHandle handle = findReusableDescriptorSet(layout);
    if (handle.isValid()) {
        m_descriptorSets[handle.id - 1].inUse = true;
        m_descriptorSets[handle.id - 1].resources = resources;
        return handle;
    }

    // Create new if none available
    return createNewDescriptorSet(layout, resources);
}

void DescriptorAllocator::releaseDescriptorSet(DescriptorSetHandle handle) {
    releaseResource(handle);
}

VkDescriptorSet DescriptorAllocator::getDescriptorSet(DescriptorSetHandle handle) const {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) {
        return VK_NULL_HANDLE;
    }
    return m_descriptorSets[handle.id - 1].descriptorSet;
}

// Resource management methods
void DescriptorAllocator::bindUniformBuffer(DescriptorSetHandle handle, SmartHandle<BufferHandle, Buffer> buffer) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) return;

    auto& entry = m_descriptorSets[handle.id - 1];
    if (!entry.inUse) return;

    entry.resources.uniformBuffers.push_back(std::move(buffer));
}

void DescriptorAllocator::bindSampler(DescriptorSetHandle handle, SamplerHandle sampler) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) return;

    auto& entry = m_descriptorSets[handle.id - 1];
    if (!entry.inUse) return;

    entry.resources.samplers.push_back(sampler);
}

void DescriptorAllocator::bindResources(DescriptorSetHandle handle, const DescriptorResources& resources) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) return;

    auto& entry = m_descriptorSets[handle.id - 1];
    if (!entry.inUse) return;

    entry.resources = resources;
}

const DescriptorAllocator::DescriptorResources& DescriptorAllocator::getDescriptorResources(DescriptorSetHandle handle) const {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) {
        return s_emptyResources;
    }

    const auto& entry = m_descriptorSets[handle.id - 1];
    if (!entry.inUse) {
        return s_emptyResources;
    }

    return entry.resources;
}

DescriptorAllocator::DescriptorResources& DescriptorAllocator::getDescriptorResources(DescriptorSetHandle handle) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) {
        static DescriptorResources emptyResources;
        emptyResources.clear();
        return emptyResources;
    }

    auto& entry = m_descriptorSets[handle.id - 1];
    if (!entry.inUse) {
        static DescriptorResources emptyResources;
        emptyResources.clear();
        return emptyResources;
    }

    return entry.resources;
}

// IResourceManager interface implementation
VkDescriptorSet* DescriptorAllocator::getResource(DescriptorSetHandle handle) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) {
        return nullptr;
    }

    VkDescriptorSet descriptorSet = m_descriptorSets[handle.id - 1].descriptorSet;
    m_resourceCache[handle] = descriptorSet;
    return &m_resourceCache[handle];
}

bool DescriptorAllocator::isValid(DescriptorSetHandle handle) const {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) {
        return false;
    }
    return m_descriptorSets[handle.id - 1].isAllocated;
}

void DescriptorAllocator::releaseResource(DescriptorSetHandle handle) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) return;

    auto& entry = m_descriptorSets[handle.id - 1];
    if (!entry.inUse) return;

    if (entry.referenceCount == 0) {
        entry.inUse = false;
        entry.resources.clear();

        // If not used by GPU, make it immediately reusable
        if (!entry.usedByGPU) {
            m_reusableSets[entry.layout].push(handle);
        }
        // If used by GPU, it will be made reusable when frame completes

        m_resourceCache.erase(handle);
    }
}

void DescriptorAllocator::addReference(DescriptorSetHandle handle) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) return;

    auto& entry = m_descriptorSets[handle.id - 1];
    entry.referenceCount++;
}

void DescriptorAllocator::removeReference(DescriptorSetHandle handle) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) return;

    auto& entry = m_descriptorSets[handle.id - 1];
    if (entry.referenceCount > 0) {
        entry.referenceCount--;

        if (entry.referenceCount == 0 && entry.inUse) {
            entry.inUse = false;
            entry.resources.clear();

            // If not used by GPU, make it immediately reusable
            if (!entry.usedByGPU) {
                m_reusableSets[entry.layout].push(handle);
            }
            // If used by GPU, it will be made reusable when frame completes

            m_resourceCache.erase(handle);
        }
    }
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> DescriptorAllocator::acquireSmartDescriptorSet(VkDescriptorSetLayout layout) {
    return acquireSmartDescriptorSet(layout, DescriptorResources{});
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> DescriptorAllocator::acquireSmartDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources) {
    DescriptorSetHandle handle = acquireDescriptorSet(layout, resources);
    return createSmartHandle(handle);
}

// Private implementation methods
DescriptorSetHandle DescriptorAllocator::findReusableDescriptorSet(VkDescriptorSetLayout layout) {
    auto& queue = m_reusableSets[layout];

    while (!queue.empty()) {
        DescriptorSetHandle handle = queue.front();
        queue.pop();

        // Check if handle is still valid and allocated
        if (handle.isValid() && handle.id <= m_descriptorSets.size()) {
            auto& entry = m_descriptorSets[handle.id - 1];
            if (entry.isAllocated && entry.layout == layout && !entry.usedByGPU) {
                return handle;
            }
        }
    }

    return DescriptorSetHandle{};
}

DescriptorSetHandle DescriptorAllocator::createNewDescriptorSet(VkDescriptorSetLayout layout) {
    return createNewDescriptorSet(layout, DescriptorResources{});
}

DescriptorSetHandle DescriptorAllocator::createNewDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources) {
    VkDescriptorPool pool = getPool();

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };

    VkDescriptorSet descriptorSet;
    VkResult result = vkAllocateDescriptorSets(m_device.get(), &allocInfo, &descriptorSet);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY) {
        // Move pool to full pools
        m_fullPools.push_back(pool);

        // Get new pool
        pool = getPool();
        allocInfo.descriptorPool = pool;
        result = vkAllocateDescriptorSets(m_device.get(), &allocInfo, &descriptorSet);
        VK_CHECK(result);
    }

    // Return pool to ready pools (can be reused)
    m_readyPools.push_back(pool);

    // Create new handle
    DescriptorSetHandle handle{ m_nextHandleId++ };

    // Expand vector if needed
    if (m_descriptorSets.size() < handle.id) {
        m_descriptorSets.resize(handle.id);
    }

    m_descriptorSets[handle.id - 1] = {
        .descriptorSet = descriptorSet,
        .layout = layout,
        .sourcePool = pool,
        .inUse = true,
        .isAllocated = true,
        .usedByGPU = false,
        .gpuFrameIndex = 0,
        .referenceCount = 0,
        .resources = resources
    };

    return handle;
}

void DescriptorAllocator::destroy() {
    // Check if already destroyed
    if (m_readyPools.empty() && m_fullPools.empty() && m_descriptorSets.empty()) {
        return;
    }

    // Ensure all GPU work is complete before destroying anything
    vkDeviceWaitIdle(m_device.get());

    // Clear all data structures
    m_descriptorSets.clear();
    m_reusableSets.clear();
    m_resourceCache.clear();
    m_frameGpuUsage.clear();

    // Destroy all pools
    for (auto pool : m_readyPools) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device.get(), pool, nullptr);
        }
    }
    for (auto pool : m_fullPools) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device.get(), pool, nullptr);
        }
    }

    m_readyPools.clear();
    m_fullPools.clear();

    // Reset counters
    m_nextHandleId = 1;
    m_nextSetCount = m_config.initialSets;
}

void DescriptorAllocator::reset() {
    // Clear all bound resources
    for (auto& entry : m_descriptorSets) {
        if (entry.inUse) {
            entry.resources.clear();
            entry.referenceCount = 0;
            entry.inUse = false;
            entry.usedByGPU = false;
            entry.gpuFrameIndex = 0;
        }
    }

    // Reset pools
    for (auto pool : m_readyPools) {
        if (pool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(m_device.get(), pool, 0);
        }
    }
    for (auto pool : m_fullPools) {
        if (pool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(m_device.get(), pool, 0);
            m_readyPools.push_back(pool);
        }
    }
    m_fullPools.clear();

    // Clear all handles and queues
    m_descriptorSets.clear();
    m_reusableSets.clear();
    m_resourceCache.clear();
    m_frameGpuUsage.clear();
    m_nextHandleId = 1;
}

VkResult DescriptorAllocator::createPool(uint32_t setCount, VkDescriptorPool* outPool) const {
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const auto& ratio : m_config.ratios) {
        uint32_t descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount);
        if (descriptorCount == 0) continue;

        poolSizes.push_back({
            .type = ratio.type,
            .descriptorCount = descriptorCount
            });
    }

    if (poolSizes.empty()) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = setCount,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    return vkCreateDescriptorPool(m_device.get(), &poolInfo, nullptr, outPool);
}

VkDescriptorPool DescriptorAllocator::getPool() {
    // Use ready pool if available
    if (!m_readyPools.empty()) {
        VkDescriptorPool pool = m_readyPools.back();
        m_readyPools.pop_back();
        return pool;
    }

    // Try to reset a full pool
    if (!m_fullPools.empty()) {
        VkDescriptorPool pool = m_fullPools.back();
        m_fullPools.pop_back();

        vkResetDescriptorPool(m_device.get(), pool, 0);

        // Mark all descriptor sets from this pool as unallocated
        for (auto& entry : m_descriptorSets) {
            if (entry.sourcePool == pool && entry.isAllocated) {
                entry.isAllocated = false;
                entry.inUse = false;
                entry.usedByGPU = false;
                entry.gpuFrameIndex = 0;
                entry.referenceCount = 0;
                entry.resources.clear();
                entry.descriptorSet = VK_NULL_HANDLE;
            }
        }

        return pool;
    }

    // Create new pool as last resort
    const uint32_t minSetCount = computeMinSetCount();
    uint32_t newSetCount = m_nextSetCount;

    while (newSetCount >= minSetCount) {
        VkDescriptorPool pool;
        VkResult result = createPool(newSetCount, &pool);

        if (result == VK_SUCCESS) {
            m_nextSetCount = static_cast<uint32_t>(newSetCount * m_config.growthFactor);
            return pool;
        }
        else if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
            newSetCount = std::max(newSetCount / 2, minSetCount);
        }
        else {
            VK_CHECK(result);
        }
    }

    VK_CHECK(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    return VK_NULL_HANDLE;
}

uint32_t DescriptorAllocator::computeMinSetCount() const {
    float minSets = 1.0f;
    for (const auto& ratio : m_config.ratios) {
        if (ratio.ratio <= 0.0f) continue;
        minSets = std::max(minSets, std::ceil(1.0f / ratio.ratio));
    }
    return static_cast<uint32_t>(minSets);
}
