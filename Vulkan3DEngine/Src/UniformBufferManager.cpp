#include "UniformBufferManager.h"
#include <stdexcept>
#include <algorithm>

UniformBufferManager::UniformBufferManager(VramManager& vramManager)
    : m_vramManager(vramManager), m_nextHandleId(1) {
}

UniformBufferManager::~UniformBufferManager() {
    for (auto& [handle, info] : m_buffers) {
        m_vramManager.freeResource(info.vramHandle);
    }
    m_buffers.clear();
    m_bufferPool.clear();
    m_resourceCache.clear();
}

UniformBufferHandle UniformBufferManager::createNewBuffer(const ShaderLib::UniformBufferObject& uboInfo) {
    UniformBufferHandle handle(m_nextHandleId++);

    VramHandle vramHandle = m_vramManager.createBuffer(
        uboInfo.size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (!vramHandle) {
        SPDLOG_ERROR("Failed to create VRAM buffer for UBO '{}'", uboInfo.name);
        return UniformBufferHandle(0);
    }

    SPDLOG_DEBUG("Created new uniform buffer '{}' with size {}", uboInfo.name, uboInfo.size);

    UniformBufferInfo bufferInfo;
    bufferInfo.vramHandle = vramHandle;
    bufferInfo.name = uboInfo.name;
    bufferInfo.size = uboInfo.size;
    bufferInfo.inUse = true;
    bufferInfo.referenceCount = 0;
    bufferInfo.variables = uboInfo.variables;

    m_buffers[handle] = std::move(bufferInfo);

    return handle;
}

UniformBufferHandle UniformBufferManager::findReusableBuffer(const ShaderLib::UniformBufferObject& uboInfo) {
    BufferPoolKey key{ uboInfo.name, uboInfo.size };
    auto it = m_bufferPool.find(key);

    if (it != m_bufferPool.end() && !it->second.empty()) {
        UniformBufferHandle handle = it->second.front();
        it->second.pop_front();

        auto& bufferInfo = m_buffers[handle];
        bufferInfo.inUse = true;

        SPDLOG_DEBUG("Reusing existing uniform buffer '{}' from pool", uboInfo.name);
        return handle;
    }

    return UniformBufferHandle(0); // Invalid handle
}

UniformBufferHandle UniformBufferManager::acquireBuffer(const ShaderLib::UniformBufferObject& uboInfo) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    // Najpierw sprawdź czy można użyć istniejącego bufora z puli
    UniformBufferHandle handle = findReusableBuffer(uboInfo);

    if (!handle.isValid()) {
        // Jeśli nie ma dostępnego bufora w puli, utwórz nowy
        SPDLOG_DEBUG("No available buffer in pool for '{}', creating new", uboInfo.name);
        handle = createNewBuffer(uboInfo);
    }

    return handle;
}

void UniformBufferManager::releaseBuffer(UniformBufferHandle handle) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    if (!isValid(handle)) {
        SPDLOG_WARN("Attempted to release invalid buffer handle: {}", handle.id);
        return;
    }

    auto& bufferInfo = m_buffers[handle];

    // Sprawdź czy bufor nie ma referencji ze smart handle'ów
    if (bufferInfo.referenceCount > 0) {
        SPDLOG_WARN("Attempted to release buffer '{}' with {} active references",
            bufferInfo.name, bufferInfo.referenceCount);
        return;
    }

    bufferInfo.inUse = false;

    BufferPoolKey key{ bufferInfo.name, bufferInfo.size };
    m_bufferPool[key].push_back(handle);

    // Wyczyść cache
    m_resourceCache.erase(handle);

    SPDLOG_DEBUG("Released uniform buffer '{}' back to pool", bufferInfo.name);
}

void UniformBufferManager::updateBuffer(UniformBufferHandle handle, const void* data, uint32_t size, uint32_t offset) {
    if (!isValid(handle)) {
        SPDLOG_WARN("Attempted to update invalid buffer handle: {}", handle.id);
        return;
    }

    auto& bufferInfo = m_buffers[handle];

    if (offset + size > bufferInfo.size) {
        SPDLOG_ERROR("Buffer update exceeds buffer size for '{}': offset {} + size {} > buffer size {}",
            bufferInfo.name, offset, size, bufferInfo.size);
        throw std::out_of_range("Buffer update exceeds buffer size");
    }

    Buffer* buffer = m_vramManager.getResource<Buffer>(bufferInfo.vramHandle);
    if (!buffer) {
        SPDLOG_ERROR("Failed to get buffer resource for '{}'", bufferInfo.name);
        throw std::runtime_error("Failed to get buffer resource");
    }

    void* mappedData = buffer->map();
    if (mappedData) {
        char* dst = static_cast<char*>(mappedData) + offset;
        std::memcpy(dst, data, size);
        buffer->unmap();
    }
    else {
        SPDLOG_ERROR("Failed to map buffer '{}' for updating", bufferInfo.name);
    }
}

SmartHandle<UniformBufferHandle, Buffer> UniformBufferManager::acquireSmartBuffer(const ShaderLib::UniformBufferObject& uboInfo) {
    UniformBufferHandle handle = acquireBuffer(uboInfo);
    return createSmartHandle(handle);
}

// IResourceManager interface implementation
Buffer* UniformBufferManager::getResource(UniformBufferHandle handle) {
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

bool UniformBufferManager::isValid(UniformBufferHandle handle) const {
    return handle.isValid() && m_buffers.find(handle) != m_buffers.end();
}

void UniformBufferManager::releaseResource(UniformBufferHandle handle) {
    releaseBuffer(handle);
}

void UniformBufferManager::addReference(UniformBufferHandle handle) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    if (isValid(handle)) {
        m_buffers[handle].referenceCount++;
        SPDLOG_DEBUG("Added reference to buffer '{}', ref count: {}",
            m_buffers[handle].name, m_buffers[handle].referenceCount);
    }
}

void UniformBufferManager::removeReference(UniformBufferHandle handle) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    if (!isValid(handle)) {
        return;
    }

    auto& bufferInfo = m_buffers[handle];
    if (bufferInfo.referenceCount > 0) {
        bufferInfo.referenceCount--;
        SPDLOG_DEBUG("Removed reference from buffer '{}', ref count: {}",
            bufferInfo.name, bufferInfo.referenceCount);

        // Jeśli spadła liczba referencji do 0 i bufor jest w użyciu, zwróć go do puli
        if (bufferInfo.referenceCount == 0 && bufferInfo.inUse) {
            bufferInfo.inUse = false;

            BufferPoolKey key{ bufferInfo.name, bufferInfo.size };
            m_bufferPool[key].push_back(handle);

            // Wyczyść cache
            m_resourceCache.erase(handle);

            SPDLOG_DEBUG("Buffer '{}' returned to pool due to zero references", bufferInfo.name);
        }
    }
}

const UniformBufferInfo& UniformBufferManager::getBufferInfo(UniformBufferHandle handle) const {
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end()) {
        SPDLOG_ERROR("Invalid uniform buffer handle: {}", handle.id);
        throw std::runtime_error("Invalid uniform buffer handle");
    }
    return it->second;
}

UniformBufferInfo& UniformBufferManager::getBufferInfo(UniformBufferHandle handle) {
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end()) {
        SPDLOG_ERROR("Invalid uniform buffer handle: {}", handle.id);
        throw std::runtime_error("Invalid uniform buffer handle");
    }
    return it->second;
}

void UniformBufferManager::cleanupUnusedBuffers(uint64_t timeThreshold) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    const size_t bufferesToKeep = 5;
    size_t totalRemoved = 0;

    for (auto& [key, pool] : m_bufferPool) {
        if (pool.size() > bufferesToKeep) {
            size_t buffersToRemove = pool.size() - bufferesToKeep;
            totalRemoved += buffersToRemove;

            for (size_t i = 0; i < buffersToRemove; ++i) {
                UniformBufferHandle handle = pool.back();
                pool.pop_back();

                // Wyczyść cache
                m_resourceCache.erase(handle);

                m_vramManager.freeResource(m_buffers[handle].vramHandle);
                m_buffers.erase(handle);
            }

            SPDLOG_INFO("Cleaned up {} excess uniform buffers for '{}'", buffersToRemove, key.name);
        }
    }

    if (totalRemoved > 0) {
        SPDLOG_INFO("Total uniform buffers cleaned up: {}", totalRemoved);
    }
}