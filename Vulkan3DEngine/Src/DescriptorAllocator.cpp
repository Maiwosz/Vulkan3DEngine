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
    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_config.framesInFlight;
}

void DescriptorAllocator::updateFramesInFlight(uint32_t newFrameCount) {
    if (newFrameCount == m_config.framesInFlight) {
        return;
    }

    // Jeśli zwiększamy liczbę klatek, inicjalizujemy nowe
    if (newFrameCount > m_config.framesInFlight) {
        size_t oldSize = m_frameData.size();
        m_frameData.resize(newFrameCount);

        for (size_t i = oldSize; i < newFrameCount; ++i) {
            m_frameData[i].nextSetCount = m_config.initialSets;
            // Nie tworzymy od razu puli - będą utworzone gdy potrzebne
        }
    }
    // Jeśli zmniejszamy liczbę klatek, przenosimy pule do legacy
    else {
        for (uint32_t i = newFrameCount; i < m_config.framesInFlight; ++i) {
            if (i < m_frameData.size()) {
                auto& frameData = m_frameData[i];

                // Przenoś pule do legacy pools zamiast je niszczyć
                m_readyPools.insert(m_readyPools.end(),
                    frameData.readyPools.begin(), frameData.readyPools.end());
                m_readyPools.insert(m_readyPools.end(),
                    frameData.fullPools.begin(), frameData.fullPools.end());

                // Przenoś reusable sets do legacy
                for (auto& [layout, queue] : frameData.reusableSets) {
                    while (!queue.empty()) {
                        m_reusableSets[layout].push(queue.front());
                        queue.pop();
                    }
                }

                frameData.readyPools.clear();
                frameData.fullPools.clear();
                frameData.reusableSets.clear();
            }
        }
        m_frameData.resize(newFrameCount);
    }

    m_config.framesInFlight = newFrameCount;

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
    return m_descriptorSets[handle.id - 1].isAllocated; // Używaj isAllocated zamiast inUse
}

void DescriptorAllocator::releaseResource(DescriptorSetHandle handle) {
    if (!handle.isValid() || handle.id > m_descriptorSets.size()) return;

    auto& entry = m_descriptorSets[handle.id - 1];
    if (!entry.inUse) return;

    if (entry.referenceCount == 0) {
        entry.inUse = false;
        entry.resources.clear();
        // isAllocated pozostaje true

        uint32_t frameIndex = entry.frameIndex;
        if (frameIndex < m_frameData.size()) {
            m_frameData[frameIndex].reusableSets[entry.layout].push(handle);
        }
        else {
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

        if (entry.referenceCount == 0 && entry.inUse) {
            entry.inUse = false;  // Oznacz jako nieużywany
            entry.resources.clear();
            // isAllocated pozostaje true - descriptor set nadal jest ważny!

            uint32_t frameIndex = entry.frameIndex;
            if (frameIndex < m_frameData.size()) {
                m_frameData[frameIndex].reusableSets[entry.layout].push(handle);
            }
            else {
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
    // Sprawdź pule dla konkretnej klatki
    if (frameIndex < m_frameData.size()) {
        auto& queue = m_frameData[frameIndex].reusableSets[layout];
        while (!queue.empty()) {
            DescriptorSetHandle handle = queue.front();
            queue.pop();

            // Sprawdź czy handle jest nadal ważny i przydzielony
            if (handle.isValid() && handle.id <= m_descriptorSets.size()) {
                auto& entry = m_descriptorSets[handle.id - 1];
                if (entry.isAllocated && entry.layout == layout) {
                    return handle;
                }
            }
        }
    }

    // Fallback do legacy reusable sets
    auto& queue = m_reusableSets[layout];
    while (!queue.empty()) {
        DescriptorSetHandle handle = queue.front();
        queue.pop();

        // Sprawdź czy handle jest nadal ważny i przydzielony
        if (handle.isValid() && handle.id <= m_descriptorSets.size()) {
            auto& entry = m_descriptorSets[handle.id - 1];
            if (entry.isAllocated && entry.layout == layout) {
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
        // Przenieś pulę do pełnych puli
        if (m_currentFrameIndex < m_frameData.size()) {
            m_frameData[m_currentFrameIndex].fullPools.push_back(pool);
        }
        else {
            m_fullPools.push_back(pool);
        }

        // Pobierz nową pulę
        pool = getPool(m_currentFrameIndex);
        allocInfo.descriptorPool = pool;
        result = vkAllocateDescriptorSets(m_device.get(), &allocInfo, &descriptorSet);
        VK_CHECK(result);
    }

    // Zwróć pulę do gotowych puli (może być użyta ponownie)
    if (m_currentFrameIndex < m_frameData.size()) {
        m_frameData[m_currentFrameIndex].readyPools.push_back(pool);
    }
    else {
        m_readyPools.push_back(pool);
    }

    // Utwórz nowy handle
    DescriptorSetHandle handle{ m_nextHandleId++ };

    // Rozszerz wektor jeśli potrzeba
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
        .frameIndex = m_currentFrameIndex,
        .resources = resources
    };

    return handle;
}

void DescriptorAllocator::resetFramePools(uint32_t frameIndex) {
    if (frameIndex >= m_frameData.size()) return;

    auto& frameData = m_frameData[frameIndex];

    vkDeviceWaitIdle(m_device.get());

    // Oznacz descriptor sets jako nieprzydzielone PRZED resetem poolów
    for (auto& entry : m_descriptorSets) {
        if (entry.frameIndex == frameIndex && entry.isAllocated) {
            entry.inUse = false;
            entry.isAllocated = false;  // Oznacz jako nieprzydzielony
            entry.referenceCount = 0;
            entry.resources.clear();
            entry.descriptorSet = VK_NULL_HANDLE;  // Invalidate VkDescriptorSet
        }
    }

    frameData.reusableSets.clear();

    // Reset poolów
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
            entry.isAllocated = false;
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
    // Sprawdź dostępne pule dla konkretnej klatki
    if (frameIndex < m_frameData.size()) {
        auto& frameData = m_frameData[frameIndex];

        // Użyj gotowej puli jeśli dostępna
        if (!frameData.readyPools.empty()) {
            VkDescriptorPool pool = frameData.readyPools.back();
            frameData.readyPools.pop_back();
            return pool;
        }

        // Sprawdź czy można zresetować pełną pulę
        if (!frameData.fullPools.empty()) {
            VkDescriptorPool pool = frameData.fullPools.back();
            frameData.fullPools.pop_back();

            // Resetuj pulę tylko jeśli nie ma innych opcji
            vkResetDescriptorPool(m_device.get(), pool, 0);

            // Oznacz wszystkie descriptor sets z tej puli jako nieprzydzielone
            for (auto& entry : m_descriptorSets) {
                if (entry.sourcePool == pool && entry.isAllocated) {
                    entry.isAllocated = false;
                    entry.inUse = false;
                    entry.referenceCount = 0;
                    entry.resources.clear();
                    entry.descriptorSet = VK_NULL_HANDLE;
                }
            }

            return pool;
        }

        // Utwórz nową pulę jako ostatnią opcję
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

    // Fallback do legacy pools z tą samą logiką
    if (!m_readyPools.empty()) {
        VkDescriptorPool pool = m_readyPools.back();
        m_readyPools.pop_back();
        return pool;
    }

    if (!m_fullPools.empty()) {
        VkDescriptorPool pool = m_fullPools.back();
        m_fullPools.pop_back();

        vkResetDescriptorPool(m_device.get(), pool, 0);

        // Oznacz wszystkie descriptor sets z tej puli jako nieprzydzielone
        for (auto& entry : m_descriptorSets) {
            if (entry.sourcePool == pool && entry.isAllocated) {
                entry.isAllocated = false;
                entry.inUse = false;
                entry.referenceCount = 0;
                entry.resources.clear();
                entry.descriptorSet = VK_NULL_HANDLE;
            }
        }

        return pool;
    }

    // Utwórz nową pulę jako ostatnią opcję
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