#include "UniformBufferManager.h"
#include <stdexcept>
#include <algorithm>

UniformBufferManager::UniformBufferManager(VramManager& vramManager)
    : m_vramManager(vramManager) {
}

UniformBufferManager::~UniformBufferManager() {
    for (auto& [handle, info] : m_buffers) {
        m_vramManager.freeResource(info.vramHandle);
    }
    m_buffers.clear();
    m_bufferPool.clear();
}

UniformBufferHandle UniformBufferManager::createBuffer(const ShaderLib::UniformBufferObject& uboInfo) {
    UniformBufferHandle handle(m_nextHandle++);

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
    bufferInfo.isInUse = true;
    bufferInfo.variables = uboInfo.variables;

    m_buffers[handle] = std::move(bufferInfo);

    return handle;
}

UniformBufferHandle UniformBufferManager::acquireBuffer(const ShaderLib::UniformBufferObject& uboInfo) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    BufferPoolKey key{ uboInfo.name, uboInfo.size };
    auto& bufferPool = m_bufferPool[key];

    if (!bufferPool.empty()) {
        UniformBufferHandle handle = bufferPool.front();
        bufferPool.pop_front();

        auto& bufferInfo = m_buffers[handle];
        bufferInfo.isInUse = true;

        SPDLOG_DEBUG("Reusing existing uniform buffer '{}' from pool", uboInfo.name);
        return handle;
    }

    SPDLOG_DEBUG("No available buffer in pool for '{}', creating new", uboInfo.name);
    return createBuffer(uboInfo);
}

void UniformBufferManager::releaseBuffer(UniformBufferHandle handle) {
    std::lock_guard<std::mutex> lock(m_poolMutex);

    if (!isBufferValid(handle)) {
        SPDLOG_WARN("Attempted to release invalid buffer handle: {}", handle.id);
        return;
    }

    auto& bufferInfo = m_buffers[handle];
    bufferInfo.isInUse = false;

    BufferPoolKey key{ bufferInfo.name, bufferInfo.size };
    m_bufferPool[key].push_back(handle);

    SPDLOG_DEBUG("Released uniform buffer '{}' back to pool", bufferInfo.name);
}

void UniformBufferManager::updateBuffer(UniformBufferHandle handle, const void* data, uint32_t size, uint32_t offset) {
    if (!isBufferValid(handle)) {
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

bool UniformBufferManager::isBufferValid(UniformBufferHandle handle) const {
    return m_buffers.find(handle) != m_buffers.end();
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

Buffer* UniformBufferManager::getBuffer(UniformBufferHandle handle) {
    if (!isBufferValid(handle)) {
        SPDLOG_WARN("Attempted to get buffer from invalid handle: {}", handle.id);
        return nullptr;
    }

    return m_vramManager.getResource<Buffer>(m_buffers[handle].vramHandle);
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