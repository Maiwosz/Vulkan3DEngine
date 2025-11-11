#include "DescriptorAllocator.h"
#include "Prerequisites.h"
#include <spdlog/spdlog.h>

// Static member definition
const DescriptorAllocator::DescriptorResources DescriptorAllocator::s_emptyResources;

DescriptorAllocator::DescriptorAllocator(const LogicalDevice& device, const PoolConfig& config) :
    m_device(device), m_config(config), m_nextSetCount(config.initialSets), m_nextHandleId(1)
{
    // Initialize with first pool
    m_readyPools.push_back(getPool());

    SPDLOG_INFO("DescriptorAllocator initialized with {} initial sets", config.initialSets);
}

DescriptorAllocator::~DescriptorAllocator() {
    destroy();
}

// ========== PUBLIC INTERFACE ==========

DescriptorSetHandle DescriptorAllocator::acquireDescriptorSet(VkDescriptorSetLayout layout) {
    return acquireDescriptorSet(layout, DescriptorResources{});
}

DescriptorSetHandle DescriptorAllocator::acquireDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources) {
    // Try to find reusable descriptor set
    DescriptorSetHandle handle = findReusableDescriptorSet(layout);
    if (handle.isValid()) {
        auto& entry = m_descriptorSets[handle.id - 1];
        entry.inUse = true;
        entry.resources = resources;

        SPDLOG_TRACE("Reusing descriptor set {} for layout {:p}", handle.id, (void*)layout);
        return handle;
    }

    // Create new if none available
    SPDLOG_TRACE("Creating new descriptor set for layout {:p}", (void*)layout);
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

// ========== RESOURCE MANAGEMENT ==========

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

// ========== SMART HANDLE SUPPORT ==========

SmartHandle<DescriptorSetHandle, VkDescriptorSet> DescriptorAllocator::acquireSmartDescriptorSet(VkDescriptorSetLayout layout) {
    return acquireSmartDescriptorSet(layout, DescriptorResources{});
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> DescriptorAllocator::acquireSmartDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources) {
    DescriptorSetHandle handle = acquireDescriptorSet(layout, resources);
    return createSmartHandle(handle);
}

// ========== IRESOURCEMANAGER INTERFACE ==========

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

    // Only release if reference count is zero
    if (entry.referenceCount == 0) {
        entry.inUse = false;
        entry.resources.clear();

        // Immediately make it reusable
        // DescriptorSetGuard will handle GPU tracking
        m_reusableSets[entry.layout].push(handle);

        SPDLOG_TRACE("Released descriptor set {} back to reusable pool", handle.id);

        m_resourceCache.erase(handle);
    }
}

void DescriptorAllocator::addReference(DescriptorSetHandle handle) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) return;

    auto& entry = m_descriptorSets[handle.id - 1];
    entry.referenceCount++;

    SPDLOG_TRACE("Descriptor set {} reference count increased to {}", handle.id, entry.referenceCount);
}

void DescriptorAllocator::removeReference(DescriptorSetHandle handle) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) return;

    auto& entry = m_descriptorSets[handle.id - 1];
    if (entry.referenceCount > 0) {
        entry.referenceCount--;

        SPDLOG_TRACE("Descriptor set {} reference count decreased to {}", handle.id, entry.referenceCount);

        // If reference count reaches zero and not in use, release
        if (entry.referenceCount == 0 && entry.inUse) {
            entry.inUse = false;
            entry.resources.clear();

            // Make it reusable
            m_reusableSets[entry.layout].push(handle);

            SPDLOG_TRACE("Descriptor set {} released after reference count reached zero", handle.id);

            m_resourceCache.erase(handle);
        }
    }
}

// ========== STATISTICS ==========

DescriptorAllocator::Stats DescriptorAllocator::getStats() const {
    Stats stats;
    stats.poolCount = m_readyPools.size() + m_fullPools.size();

    for (const auto& entry : m_descriptorSets) {
        if (entry.isAllocated) {
            stats.totalAllocated++;
            if (entry.inUse) {
                stats.inUse++;
            }
        }
    }

    for (const auto& [layout, queue] : m_reusableSets) {
        stats.reusable += queue.size();
    }

    return stats;
}

// ========== PRIVATE IMPLEMENTATION ==========

DescriptorSetHandle DescriptorAllocator::findReusableDescriptorSet(VkDescriptorSetLayout layout) {
    auto& queue = m_reusableSets[layout];

    while (!queue.empty()) {
        DescriptorSetHandle handle = queue.front();
        queue.pop();

        // Check if handle is still valid and allocated
        if (handle.isValid() && handle.id <= m_descriptorSets.size()) {
            auto& entry = m_descriptorSets[handle.id - 1];
            if (entry.isAllocated && entry.layout == layout && !entry.inUse) {
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

        SPDLOG_DEBUG("Pool exhausted, moving to full pools. Creating new pool...");

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
        .referenceCount = 0,
        .resources = resources
    };

    SPDLOG_TRACE("Created new descriptor set {} from pool {:p}", handle.id, (void*)pool);

    return handle;
}

// ========== LIFECYCLE MANAGEMENT ==========

void DescriptorAllocator::destroy() {
    // Check if already destroyed
    if (m_readyPools.empty() && m_fullPools.empty() && m_descriptorSets.empty()) {
        return;
    }

    SPDLOG_INFO("Destroying DescriptorAllocator...");

    // Ensure all GPU work is complete before destroying anything
    vkDeviceWaitIdle(m_device.get());

    // Clear all data structures
    m_descriptorSets.clear();
    m_reusableSets.clear();
    m_resourceCache.clear();

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

    SPDLOG_INFO("DescriptorAllocator destroyed");
}

void DescriptorAllocator::reset() {
    SPDLOG_INFO("Resetting DescriptorAllocator...");

    // Clear all bound resources
    for (auto& entry : m_descriptorSets) {
        if (entry.inUse) {
            entry.resources.clear();
            entry.referenceCount = 0;
            entry.inUse = false;
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
    m_nextHandleId = 1;

    SPDLOG_INFO("DescriptorAllocator reset complete");
}

// ========== POOL MANAGEMENT ==========

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
        SPDLOG_ERROR("Cannot create descriptor pool - no valid pool sizes");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = setCount,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VkResult result = vkCreateDescriptorPool(m_device.get(), &poolInfo, nullptr, outPool);

    if (result == VK_SUCCESS) {
        SPDLOG_DEBUG("Created descriptor pool {:p} with {} max sets", (void*)*outPool, setCount);
    }
    else {
        SPDLOG_ERROR("Failed to create descriptor pool with {} sets: {}", setCount, string_VkResult(result));
    }

    return result;
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

        SPDLOG_DEBUG("Resetting full pool {:p}", (void*)pool);
        vkResetDescriptorPool(m_device.get(), pool, 0);

        // Mark all descriptor sets from this pool as unallocated
        size_t invalidatedCount = 0;
        for (auto& entry : m_descriptorSets) {
            if (entry.sourcePool == pool && entry.isAllocated) {
                entry.isAllocated = false;
                entry.inUse = false;
                entry.referenceCount = 0;
                entry.resources.clear();
                entry.descriptorSet = VK_NULL_HANDLE;
                invalidatedCount++;
            }
        }

        SPDLOG_DEBUG("Invalidated {} descriptor sets from reset pool", invalidatedCount);

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
            SPDLOG_INFO("Created new pool with {} sets, next pool will have {}", newSetCount, m_nextSetCount);
            return pool;
        }
        else if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
            SPDLOG_WARN("Failed to create pool with {} sets, trying smaller size...", newSetCount);
            newSetCount = std::max(newSetCount / 2, minSetCount);
        }
        else {
            VK_CHECK(result);
        }
    }

    SPDLOG_CRITICAL("Cannot create descriptor pool - exhausted all options");
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
