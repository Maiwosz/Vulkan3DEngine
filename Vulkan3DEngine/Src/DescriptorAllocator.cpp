#include "DescriptorAllocator.h"
#include "Prerequisites.h"

// Static member definition
const DescriptorAllocator::DescriptorResources DescriptorAllocator::s_emptyResources;

DescriptorAllocator::DescriptorAllocator(const LogicalDevice& device, const PoolConfig& config) :
    m_device(device), m_config(config), m_nextSetCount(config.initialSets), m_nextHandleId(1), m_currentFrameIndex(0)
{
    // Initialize per-frame data
    m_frameData.resize(m_config.framesInFlight);
    for (auto& frameData : m_frameData) {
        frameData.nextSetCount = config.initialSets;
        frameData.readyPools.push_back(getPool(0)); // Initialize with first frame
    }

    // Keep legacy pools for backward compatibility
    m_readyPools.push_back(getPool(0));
}

DescriptorAllocator::~DescriptorAllocator() {
    destroy();
}

// Frame management methods
void DescriptorAllocator::advanceFrame() {
    // Don't reset the frame we're advancing to immediately
    // Instead, mark it for reset but only reset when we're sure it's safe
    uint32_t nextFrame = (m_currentFrameIndex + 1) % m_config.framesInFlight;

    // The frame we're about to use should be safe to reset since it's been
    // at least framesInFlight frames since it was last used
    m_currentFrameIndex = nextFrame;

    // Reset the frame pools for the frame we just switched to
    // This is safe because this frame hasn't been used for framesInFlight frames
    resetFramePools(m_currentFrameIndex);
}

void DescriptorAllocator::updateFramesInFlight(uint32_t newFrameCount) {
    if (newFrameCount == m_config.framesInFlight) {
        return; // No change needed
    }

    // If reducing frame count, destroy excess frames
    if (newFrameCount < m_config.framesInFlight) {
        for (uint32_t i = newFrameCount; i < m_config.framesInFlight; ++i) {
            destroyFramePools(i);
        }
        m_frameData.resize(newFrameCount);
    }
    // If increasing frame count, initialize new frames
    else {
        size_t oldSize = m_frameData.size();
        m_frameData.resize(newFrameCount);

        for (size_t i = oldSize; i < newFrameCount; ++i) {
            m_frameData[i].nextSetCount = m_config.initialSets;
            m_frameData[i].readyPools.push_back(getPool(static_cast<uint32_t>(i)));
        }
    }

    m_config.framesInFlight = newFrameCount;

    // Adjust current frame index if it's now out of bounds
    if (m_currentFrameIndex >= newFrameCount) {
        m_currentFrameIndex = 0;
    }
}

// Enhanced public interface
DescriptorSetHandle DescriptorAllocator::acquireDescriptorSet(VkDescriptorSetLayout layout) {
    return acquireDescriptorSet(layout, DescriptorResources{});
}

DescriptorSetHandle DescriptorAllocator::acquireDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources) {
    // Try to find reusable descriptor from current frame first
    DescriptorSetHandle handle = findReusableDescriptorSet(layout, m_currentFrameIndex);
    if (handle.isValid()) {
        m_descriptorSets[handle.id - 1].inUse = true;
        m_descriptorSets[handle.id - 1].resources = resources; // Copy resources
        m_descriptorSets[handle.id - 1].frameIndex = m_currentFrameIndex;
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
void DescriptorAllocator::bindUniformBuffer(DescriptorSetHandle handle, SmartHandle<UniformBufferHandle, Buffer> buffer) {
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
    return m_descriptorSets[handle.id - 1].inUse;
}

void DescriptorAllocator::releaseResource(DescriptorSetHandle handle) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) return;

    auto& entry = m_descriptorSets[handle.id - 1];
    if (!entry.inUse) return;

    // Release only if no active references
    if (entry.referenceCount == 0) {
        entry.inUse = false;
        entry.resources.clear(); // Clear bound resources - this will automatically release smart handles

        // Add to appropriate frame's reusable sets
        uint32_t frameIndex = entry.frameIndex;
        if (frameIndex < m_frameData.size()) {
            m_frameData[frameIndex].reusableSets[entry.layout].push(handle);
        }
        else {
            // Fallback to legacy reusable sets
            m_reusableSets[entry.layout].push(handle);
        }

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

        // If reference count dropped to 0 and resource is in use, return to pool
        if (entry.referenceCount == 0 && entry.inUse) {
            entry.inUse = false;
            entry.resources.clear(); // Clear bound resources

            // Add to appropriate frame's reusable sets
            uint32_t frameIndex = entry.frameIndex;
            if (frameIndex < m_frameData.size()) {
                m_frameData[frameIndex].reusableSets[entry.layout].push(handle);
            }
            else {
                // Fallback to legacy reusable sets
                m_reusableSets[entry.layout].push(handle);
            }

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
DescriptorSetHandle DescriptorAllocator::findReusableDescriptorSet(VkDescriptorSetLayout layout, uint32_t frameIndex) {
    if (frameIndex < m_frameData.size()) {
        auto& queue = m_frameData[frameIndex].reusableSets[layout];
        if (!queue.empty()) {
            DescriptorSetHandle handle = queue.front();
            queue.pop();
            return handle;
        }
    }

    // Fallback to legacy reusable sets
    auto& queue = m_reusableSets[layout];
    if (!queue.empty()) {
        DescriptorSetHandle handle = queue.front();
        queue.pop();
        return handle;
    }

    return DescriptorSetHandle{};
}

DescriptorSetHandle DescriptorAllocator::createNewDescriptorSet(VkDescriptorSetLayout layout) {
    return createNewDescriptorSet(layout, DescriptorResources{});
}

DescriptorSetHandle DescriptorAllocator::createNewDescriptorSet(VkDescriptorSetLayout layout, const DescriptorResources& resources) {
    VkDescriptorPool pool = getPool(m_currentFrameIndex);

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };

    VkDescriptorSet descriptorSet;
    VkResult result = vkAllocateDescriptorSets(m_device.get(), &allocInfo, &descriptorSet);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY) {
        // Move pool to full pools for current frame
        if (m_currentFrameIndex < m_frameData.size()) {
            m_frameData[m_currentFrameIndex].fullPools.push_back(pool);
        }
        else {
            m_fullPools.push_back(pool);
        }

        pool = getPool(m_currentFrameIndex);
        allocInfo.descriptorPool = pool;
        VK_CHECK(vkAllocateDescriptorSets(m_device.get(), &allocInfo, &descriptorSet));
    }

    // Add pool back to ready pools for current frame
    if (m_currentFrameIndex < m_frameData.size()) {
        m_frameData[m_currentFrameIndex].readyPools.push_back(pool);
    }
    else {
        m_readyPools.push_back(pool);
    }

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
        .referenceCount = 0,
        .frameIndex = m_currentFrameIndex,
        .resources = resources // Copy the provided resources
    };

    return handle;
}

void DescriptorAllocator::resetFramePools(uint32_t frameIndex) {
    if (frameIndex >= m_frameData.size()) return;

    auto& frameData = m_frameData[frameIndex];

    // First, ensure all command buffers using these descriptor sets have finished
    // Wait for the specific frame's fence to be signaled before resetting pools
    vkDeviceWaitIdle(m_device.get());

    // Mark all descriptor sets from this frame as not in use BEFORE resetting pools
    for (auto& entry : m_descriptorSets) {
        if (entry.frameIndex == frameIndex && entry.inUse) {
            entry.inUse = false;
            entry.referenceCount = 0;
            entry.resources.clear();
        }
    }

    // Clear reusable sets for this frame BEFORE resetting pools
    frameData.reusableSets.clear();

    // Now it's safe to reset pools since no descriptor sets are marked as in use
    for (auto pool : frameData.readyPools) {
        if (pool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(m_device.get(), pool, 0);
        }
    }
    for (auto pool : frameData.fullPools) {
        if (pool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(m_device.get(), pool, 0);
            frameData.readyPools.push_back(pool);
        }
    }
    frameData.fullPools.clear();
}

void DescriptorAllocator::destroyFramePools(uint32_t frameIndex) {
    if (frameIndex >= m_frameData.size()) return;

    auto& frameData = m_frameData[frameIndex];

    // Destroy all pools for this frame
    for (auto pool : frameData.readyPools) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device.get(), pool, nullptr);
        }
    }
    for (auto pool : frameData.fullPools) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device.get(), pool, nullptr);
        }
    }

    frameData.readyPools.clear();
    frameData.fullPools.clear();
    frameData.reusableSets.clear();
}

void DescriptorAllocator::destroy() {
    // Check if already destroyed
    if (m_readyPools.empty() && m_fullPools.empty() && m_descriptorSets.empty() && m_frameData.empty()) {
        return; // Already destroyed
    }

    // Ensure all GPU work is complete before destroying anything
    vkDeviceWaitIdle(m_device.get());

    // 1. Explicitly release all active descriptor sets and their resources
    for (auto& entry : m_descriptorSets) {
        if (entry.inUse || entry.referenceCount > 0) {
            // Force release of all bound resources (smart handles)
            entry.resources.clear();

            // Zero out all flags and counters
            entry.referenceCount = 0;
            entry.inUse = false;
            entry.descriptorSet = VK_NULL_HANDLE;
            entry.layout = VK_NULL_HANDLE;
            entry.sourcePool = VK_NULL_HANDLE;
        }
    }

    // 2. Clear all data structures BEFORE destroying pools
    m_descriptorSets.clear();
    m_reusableSets.clear();
    m_resourceCache.clear();

    // 3. Destroy per-frame pools (no need to reset since we're destroying)
    for (uint32_t i = 0; i < m_frameData.size(); ++i) {
        auto& frameData = m_frameData[i];

        // Destroy all pools for this frame directly without resetting
        for (auto pool : frameData.readyPools) {
            if (pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(m_device.get(), pool, nullptr);
            }
        }
        for (auto pool : frameData.fullPools) {
            if (pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(m_device.get(), pool, nullptr);
            }
        }

        frameData.readyPools.clear();
        frameData.fullPools.clear();
        frameData.reusableSets.clear();
    }
    m_frameData.clear();

    // 4. Destroy all legacy pools directly without resetting
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

    // 5. Clear pool collections
    m_readyPools.clear();
    m_fullPools.clear();

    // 6. Reset all counters
    m_nextHandleId = 1;
    m_nextSetCount = m_config.initialSets;
    m_currentFrameIndex = 0;
}


void DescriptorAllocator::reset() {
    // 1. First clear all bound resources
    for (auto& entry : m_descriptorSets) {
        if (entry.inUse) {
            entry.resources.clear(); // Release smart handles
            entry.referenceCount = 0;
            entry.inUse = false;
        }
    }

    // 2. Reset all per-frame pools
    for (uint32_t i = 0; i < m_frameData.size(); ++i) {
        resetFramePools(i);
    }

    // 3. Reset legacy pools
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

    // 4. Clear all handles and queues
    m_descriptorSets.clear();
    m_reusableSets.clear();
    m_resourceCache.clear();
    m_nextHandleId = 1;
    m_currentFrameIndex = 0;
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

VkDescriptorPool DescriptorAllocator::getPool(uint32_t frameIndex) {
    // Try to get pool from specific frame first
    if (frameIndex < m_frameData.size()) {
        auto& frameData = m_frameData[frameIndex];
        if (!frameData.readyPools.empty()) {
            VkDescriptorPool pool = frameData.readyPools.back();
            frameData.readyPools.pop_back();
            return pool;
        }

        // Create new pool for this frame
        const uint32_t minSetCount = computeMinSetCount();
        uint32_t newSetCount = frameData.nextSetCount;

        while (newSetCount >= minSetCount) {
            VkDescriptorPool pool;
            VkResult result = createPool(newSetCount, &pool);

            if (result == VK_SUCCESS) {
                frameData.nextSetCount = static_cast<uint32_t>(newSetCount * m_config.growthFactor);
                return pool;
            }
            else if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
                newSetCount = std::max(newSetCount / 2, minSetCount);
            }
            else {
                VK_CHECK(result);
            }
        }
    }

    // Fallback to legacy pool creation
    if (!m_readyPools.empty()) {
        VkDescriptorPool pool = m_readyPools.back();
        m_readyPools.pop_back();
        return pool;
    }

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