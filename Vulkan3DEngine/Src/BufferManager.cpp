#include "BufferManager.h"
#include <stdexcept>
#include <algorithm>

BufferManager::BufferManager(VramManager& vramManager)
    : m_vramManager(vramManager), m_nextHandleId(1) {
}

BufferManager::~BufferManager() {
    for (auto& [handle, info] : m_buffers) {
        m_vramManager.freeResource(info.vramHandle);
    }
    m_buffers.clear();
    m_bufferPool.clear();
    m_resourceCache.clear();
}

BufferHandle BufferManager::createNewBuffer(std::shared_ptr<const ShaderLib::BufferObjectDefinition> bufferInfo) {
    BufferHandle handle(m_nextHandleId++);

    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags memoryProperties;

    if (bufferInfo->IsUniformBuffer()) {
        usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    else { // Storage Buffer
        usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    VramHandle vramHandle = m_vramManager.createBuffer(
        bufferInfo->GetTotalSize(),
        usage,
        memoryProperties
    );

    if (!vramHandle) {
        SPDLOG_ERROR("Failed to create VRAM buffer for {} '{}'",
            bufferInfo->IsUniformBuffer() ? "UBO" : "SSBO", bufferInfo->GetName());
        return BufferHandle(0);
    }

    SPDLOG_DEBUG("Created new {} buffer '{}' with size {} (layout: {})",
        bufferInfo->IsUniformBuffer() ? "uniform" : "storage",
        bufferInfo->GetName(),
        bufferInfo->GetTotalSize(),
        bufferInfo->GetLayoutStandard() == ShaderLib::LayoutStandard::Std140 ? "std140" : "std430");

    BufferInfo bufInfo;
    bufInfo.vramHandle = vramHandle;
    bufInfo.name = bufferInfo->GetName();
    bufInfo.size = bufferInfo->GetTotalSize();
    bufInfo.bufferType = bufferInfo->GetBufferType();
    bufInfo.layoutStandard = bufferInfo->GetLayoutStandard();
    bufInfo.inUse = true;
    bufInfo.referenceCount = 0;
    bufInfo.bufferObject = bufferInfo;

    m_buffers[handle] = std::move(bufInfo);

    return handle;
}

BufferHandle BufferManager::findReusableBuffer(std::shared_ptr<const ShaderLib::BufferObjectDefinition> bufferInfo) {
    BufferPoolKey key{ bufferInfo->GetName(), bufferInfo->GetTotalSize(), bufferInfo->GetBufferType()};
    auto it = m_bufferPool.find(key);

    if (it != m_bufferPool.end() && !it->second.empty()) {
        BufferHandle handle = it->second.front();
        it->second.pop_front();

        auto& bufInfo = m_buffers[handle];
        bufInfo.inUse = true;

        SPDLOG_DEBUG("Reusing existing {} buffer '{}' from pool",
            bufferInfo->IsUniformBuffer() ? "uniform" : "storage", bufferInfo->GetName());
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

    BufferPoolKey key{ bufferInfo.name, bufferInfo.size, bufferInfo.bufferType };
    m_bufferPool[key].push_back(handle);

    m_resourceCache.erase(handle);

    SPDLOG_DEBUG("Released {} buffer '{}' back to pool",
        bufferInfo.bufferType == ShaderLib::BufferType::Uniform ? "uniform" : "storage",
        bufferInfo.name);
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

            BufferPoolKey key{ bufferInfo.name, bufferInfo.size, bufferInfo.bufferType };
            m_bufferPool[key].push_back(handle);

            m_resourceCache.erase(handle);

            SPDLOG_DEBUG("Buffer '{}' returned to pool due to zero references", bufferInfo.name);
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
