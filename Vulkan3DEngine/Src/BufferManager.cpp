#include "BufferManager.h"
#include <stdexcept>
#include <algorithm>

BufferManager::BufferManager(VramManager& vramManager)
    : m_vramManager(vramManager), m_nextHandleId(1) {
}

BufferManager::~BufferManager() {
    // Unmap all persistently mapped buffers before destruction
    for (auto& [handle, info] : m_buffers) {
        if (info.isPersistentlyMapped) {
            Buffer* buffer = m_vramManager.getResource<Buffer>(info.vramHandle);
            if (buffer && buffer->isMapped()) {
                buffer->unmap();
            }
        }
        m_vramManager.freeResource(info.vramHandle);
    }
    m_buffers.clear();
    m_bufferPool.clear();
    m_resourceCache.clear();
}

void BufferManager::ensureBufferMapped(BufferHandle handle) {
    if (!isValid(handle)) {
        return;
    }

    auto& bufferInfo = m_buffers[handle];

    // Skip if already mapped
    if (bufferInfo.isPersistentlyMapped) {
        return;
    }

    // Check if buffer should be mapped based on access patterns
    if (bufferInfo.bufferObject) {
        const auto& accessPatterns = bufferInfo.bufferObject->GetAccessPatterns();

        // Only map if CPU has visible access
        if (!accessPatterns.IsCPUVisible()) {
            SPDLOG_DEBUG("Skipping mapping for '{}' - not CPU visible",
                bufferInfo.name);
            return;
        }
    }

    Buffer* buffer = m_vramManager.getResource<Buffer>(bufferInfo.vramHandle);
    if (!buffer) {
        SPDLOG_ERROR("Failed to get buffer resource for mapping: {}",
            bufferInfo.name);
        return;
    }

    // Map the buffer persistently
    try {
        buffer->map();
        bufferInfo.isPersistentlyMapped = true;

        SPDLOG_DEBUG("Persistently mapped {} buffer '{}' at address {:p}",
            bufferInfo.bufferType == ShaderLib::BufferType::Uniform
            ? "uniform" : "storage",
            bufferInfo.name,
            buffer->getMappedPointer());
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to persistently map buffer '{}': {}",
            bufferInfo.name, e.what());
        bufferInfo.isPersistentlyMapped = false;
    }
}

BufferManager::VulkanBufferConfig BufferManager::determineBufferConfig(
    std::shared_ptr<const ShaderLib::BufferObjectDefinition> bufferInfo
) const {
    VulkanBufferConfig config{};

    const auto& accessPatterns = bufferInfo->GetAccessPatterns();
    const bool isUniform = bufferInfo->IsUniformBuffer();

    // ========================================================================
    // USAGE FLAGS - Based on buffer type and GPU access
    // ========================================================================

    if (isUniform) {
        config.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    else {
        config.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    // Add transfer flags if CPU writes to buffer
    if (accessPatterns.IsCPUWritable()) {
        config.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    // ========================================================================
    // MEMORY PROPERTIES - Based on CPU/GPU access patterns
    // ========================================================================

    const bool cpuVisible = accessPatterns.IsCPUVisible();
    const bool cpuFrequent = bufferInfo->GetCPUAccessFrequency() >=
        ShaderLib::AccessFrequency::OncePerFrame;

    if (cpuVisible) {
        // CPU-accessible memory
        config.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        // Add coherent flag for frequent CPU access to avoid manual flushing
        if (cpuFrequent) {
            config.memoryProperties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }

        // Prefer device-local memory if GPU accesses frequently
        const bool gpuFrequent = bufferInfo->GetGPUAccessFrequency() >=
            ShaderLib::AccessFrequency::OncePerFrame;
        if (gpuFrequent) {
            config.memoryProperties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }

        config.vmaUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    }
    else {
        // GPU-only memory
        config.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        config.vmaUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    }

    // ========================================================================
    // PERSISTENT MAPPING STRATEGY
    // ========================================================================

    // Persistently map if:
    // 1. CPU has access AND
    // 2. (Frequent CPU access OR small buffer size for low overhead)
    const bool isSmallBuffer = bufferInfo->GetTotalSize() <= 4096; // 4KB threshold

    config.shouldPersistentlyMap = cpuVisible && (cpuFrequent || isSmallBuffer);

    SPDLOG_DEBUG("Buffer config for '{}': usage=0x{:x}, memProps=0x{:x}, "
        "vmaUsage={}, persistentMap={}, cpuFreq={}, gpuFreq={}, size={}",
        bufferInfo->GetName(),
        config.usage,
        config.memoryProperties,
        static_cast<int>(config.vmaUsage),
        config.shouldPersistentlyMap,
        static_cast<int>(bufferInfo->GetCPUAccessFrequency()),
        static_cast<int>(bufferInfo->GetGPUAccessFrequency()),
        bufferInfo->GetTotalSize());

    return config;
}

BufferHandle BufferManager::createNewBuffer(
    std::shared_ptr<const ShaderLib::BufferObjectDefinition> bufferInfo
) {
    BufferHandle handle(m_nextHandleId++);

    // Determine optimal Vulkan configuration based on access patterns
    VulkanBufferConfig config = determineBufferConfig(bufferInfo);

    VramHandle vramHandle = m_vramManager.createBuffer(
        bufferInfo->GetTotalSize(),
        config.usage,
        config.memoryProperties
    );

    if (!vramHandle) {
        SPDLOG_ERROR("Failed to create VRAM buffer for {} '{}'",
            bufferInfo->IsUniformBuffer() ? "UBO" : "SSBO",
            bufferInfo->GetName());
        return BufferHandle(0);
    }

    SPDLOG_DEBUG("Created new {} buffer '{}' with size {} (layout: {}, "
        "cpuAccess: {}, gpuAccess: {})",
        bufferInfo->IsUniformBuffer() ? "uniform" : "storage",
        bufferInfo->GetName(),
        bufferInfo->GetTotalSize(),
        bufferInfo->GetLayoutStandard() == ShaderLib::LayoutStandard::Std140
        ? "std140" : "std430",
        ShaderLib::AccessOperationToString(
            bufferInfo->GetCPUAccessProfile().operation),
        ShaderLib::AccessOperationToString(
            bufferInfo->GetGPUAccessProfile().operation));

    BufferInfo bufInfo;
    bufInfo.vramHandle = vramHandle;
    bufInfo.name = bufferInfo->GetName();
    bufInfo.size = bufferInfo->GetTotalSize();
    bufInfo.bufferType = bufferInfo->GetBufferType();
    bufInfo.layoutStandard = bufferInfo->GetLayoutStandard();
    bufInfo.inUse = true;
    bufInfo.referenceCount = 0;
    bufInfo.bufferObject = bufferInfo;
    bufInfo.isPersistentlyMapped = false;

    m_buffers[handle] = std::move(bufInfo);

    // Map only if access patterns suggest persistent mapping is beneficial
    if (config.shouldPersistentlyMap) {
        ensureBufferMapped(handle);
    }
    else {
        SPDLOG_DEBUG("Buffer '{}' not persistently mapped based on access patterns",
            bufferInfo->GetName());
    }

    return handle;
}

BufferHandle BufferManager::findReusableBuffer(
    std::shared_ptr<const ShaderLib::BufferObjectDefinition> bufferInfo
) {
    BufferPoolKey key{
        bufferInfo->GetName(),
        bufferInfo->GetTotalSize(),
        bufferInfo->GetBufferType()
    };

    auto it = m_bufferPool.find(key);

    if (it != m_bufferPool.end() && !it->second.empty()) {
        BufferHandle handle = it->second.front();
        it->second.pop_front();

        auto& bufInfo = m_buffers[handle];
        bufInfo.inUse = true;

        // Ensure proper mapping state based on access patterns
        VulkanBufferConfig config = determineBufferConfig(bufferInfo);

        if (config.shouldPersistentlyMap && !bufInfo.isPersistentlyMapped) {
            ensureBufferMapped(handle);
        }
        else if (!config.shouldPersistentlyMap && bufInfo.isPersistentlyMapped) {
            // Unmap if access patterns changed and persistent mapping not needed
            Buffer* buffer = m_vramManager.getResource<Buffer>(bufInfo.vramHandle);
            if (buffer && buffer->isMapped()) {
                buffer->unmap();
                bufInfo.isPersistentlyMapped = false;
                SPDLOG_DEBUG("Unmapped reused buffer '{}' - not needed per access patterns",
                    bufferInfo->GetName());
            }
        }

        SPDLOG_DEBUG("Reusing existing {} buffer '{}' from pool (mapped: {})",
            bufferInfo->IsUniformBuffer() ? "uniform" : "storage",
            bufferInfo->GetName(),
            bufInfo.isPersistentlyMapped);
        return handle;
    }

    return BufferHandle(0);
}

BufferHandle BufferManager::acquireBuffer(std::shared_ptr<const ShaderLib::BufferObjectDefinition> bufferInfo) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    BufferHandle handle = findReusableBuffer(bufferInfo);

    if (!handle.isValid()) {
        SPDLOG_DEBUG("No available buffer in pool for '{}', creating new", bufferInfo->GetName());
        handle = createNewBuffer(bufferInfo);
    }

    return handle;
}

void BufferManager::releaseBuffer(BufferHandle handle) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    if (!isValid(handle)) {
        SPDLOG_WARN("Attempted to release invalid buffer handle: {}", handle.id);
        return;
    }

    auto& bufferInfo = m_buffers[handle];

    if (bufferInfo.referenceCount > 0) {
        SPDLOG_WARN("Attempted to release buffer '{}' with {} active references",
            bufferInfo.name, bufferInfo.referenceCount);
        return;
    }

    bufferInfo.inUse = false;

    // Keep mapped state based on access patterns
    // Frequently accessed buffers stay mapped even in pool for fast reuse
    const bool shouldKeepMapped = bufferInfo.bufferObject &&
        bufferInfo.bufferObject->GetCPUAccessFrequency() >=
        ShaderLib::AccessFrequency::OncePerFrame;

    if (!shouldKeepMapped && bufferInfo.isPersistentlyMapped) {
        Buffer* buffer = m_vramManager.getResource<Buffer>(bufferInfo.vramHandle);
        if (buffer && buffer->isMapped()) {
            buffer->unmap();
            bufferInfo.isPersistentlyMapped = false;
            SPDLOG_DEBUG("Unmapped infrequently used buffer '{}' when returning to pool",
                bufferInfo.name);
        }
    }

    BufferPoolKey key{ bufferInfo.name, bufferInfo.size, bufferInfo.bufferType };
    m_bufferPool[key].push_back(handle);

    m_resourceCache.erase(handle);

    SPDLOG_DEBUG("Released {} buffer '{}' back to pool (mapped: {})",
        bufferInfo.bufferType == ShaderLib::BufferType::Uniform
        ? "uniform" : "storage",
        bufferInfo.name,
        bufferInfo.isPersistentlyMapped);
}

SmartHandle<BufferHandle, Buffer> BufferManager::acquireSmartBuffer(std::shared_ptr<const ShaderLib::BufferObjectDefinition> bufferInfo) {
    BufferHandle handle = acquireBuffer(bufferInfo);
    return createSmartHandle(handle);
}

Buffer* BufferManager::getResource(BufferHandle handle) {
    auto cacheIt = m_resourceCache.find(handle);
    if (cacheIt != m_resourceCache.end()) {
        return cacheIt->second;
    }

    if (!isValid(handle)) {
        return nullptr;
    }

    Buffer* buffer = m_vramManager.getResource<Buffer>(m_buffers[handle].vramHandle);
    if (buffer) {
        m_resourceCache[handle] = buffer;
    }

    return buffer;
}

bool BufferManager::isValid(BufferHandle handle) const {
    return handle.isValid() && m_buffers.find(handle) != m_buffers.end();
}

void BufferManager::releaseResource(BufferHandle handle) {
    releaseBuffer(handle);
}

void BufferManager::addReference(BufferHandle handle) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    if (isValid(handle)) {
        m_buffers[handle].referenceCount++;
        SPDLOG_DEBUG("Added reference to buffer '{}', ref count: {}",
            m_buffers[handle].name, m_buffers[handle].referenceCount);
    }
}

void BufferManager::removeReference(BufferHandle handle) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    if (!isValid(handle)) {
        return;
    }

    auto& bufferInfo = m_buffers[handle];
    if (bufferInfo.referenceCount > 0) {
        bufferInfo.referenceCount--;
        SPDLOG_DEBUG("Removed reference from buffer '{}', ref count: {}",
            bufferInfo.name, bufferInfo.referenceCount);

        if (bufferInfo.referenceCount == 0 && bufferInfo.inUse) {
            bufferInfo.inUse = false;

            // Keep persistent mapping when returning to pool
            BufferPoolKey key{ bufferInfo.name, bufferInfo.size, bufferInfo.bufferType };
            m_bufferPool[key].push_back(handle);

            m_resourceCache.erase(handle);

            SPDLOG_DEBUG("Buffer '{}' returned to pool due to zero references (keeping persistent mapping)",
                bufferInfo.name);
        }
    }
}

const BufferInfo& BufferManager::getBufferInfo(BufferHandle handle) const {
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end()) {
        SPDLOG_ERROR("Invalid buffer handle: {}", handle.id);
        throw std::runtime_error("Invalid buffer handle");
    }
    return it->second;
}

std::shared_ptr<const ShaderLib::BufferObjectDefinition> BufferManager::getBufferObject(BufferHandle handle) const {
    return getBufferInfo(handle).bufferObject;
}

void BufferManager::cleanupUnusedBuffers(uint64_t timeThreshold) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    const size_t bufferesToKeep = 5;
    size_t totalRemoved = 0;

    for (auto& [key, pool] : m_bufferPool) {
        if (pool.size() > bufferesToKeep) {
            size_t buffersToRemove = pool.size() - bufferesToKeep;
            totalRemoved += buffersToRemove;

            for (size_t i = 0; i < buffersToRemove; ++i) {
                BufferHandle handle = pool.back();
                pool.pop_back();

                // Unmap before destroying
                auto& bufferInfo = m_buffers[handle];
                if (bufferInfo.isPersistentlyMapped) {
                    Buffer* buffer = m_vramManager.getResource<Buffer>(bufferInfo.vramHandle);
                    if (buffer && buffer->isMapped()) {
                        buffer->unmap();
                    }
                }

                m_resourceCache.erase(handle);
                m_vramManager.freeResource(m_buffers[handle].vramHandle);
                m_buffers.erase(handle);
            }

            SPDLOG_INFO("Cleaned up {} excess {} buffers for '{}'",
                buffersToRemove,
                key.bufferType == ShaderLib::BufferType::Uniform ? "uniform" : "storage",
                key.name);
        }
    }

    if (totalRemoved > 0) {
        SPDLOG_INFO("Total buffers cleaned up: {}", totalRemoved);
    }
}
