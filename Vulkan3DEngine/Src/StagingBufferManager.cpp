#include "StagingBufferManager.h"
#include <algorithm>

StagingBufferManager::StagingBufferManager(VmaAllocator allocator, const LogicalDevice& device)
    : m_allocator(allocator), m_device(device) {
}

StagingBufferManager::~StagingBufferManager() {
    for (auto& buffer : m_availableBuffers) {
        buffer.destroy();
    }
    for (auto& [fence, buffers] : m_inUseBuffers) {
        for (auto& buffer : buffers) {
            buffer.destroy();
        }
    }
}

Buffer StagingBufferManager::requestBuffer(VkDeviceSize size, VkFence fence) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Szukaj bufora o wystarczaj¹cym rozmiarze w dostêpnych
    auto it = std::find_if(m_availableBuffers.begin(), m_availableBuffers.end(),
        [size](const Buffer& b) { return b.getSize() >= size; });

    Buffer buffer;
    if (it != m_availableBuffers.end()) {
        buffer = std::move(*it);
        m_availableBuffers.erase(it);
    }
    else {
        buffer = Buffer::createStaging(m_allocator, size);
    }

    m_inUseBuffers[fence].push_back(std::move(buffer));
    return buffer;
}

void StagingBufferManager::reclaimBuffers() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_inUseBuffers.begin();
    while (it != m_inUseBuffers.end()) {
        VkResult status = vkGetFenceStatus(m_device.get(), it->first);
        if (status == VK_SUCCESS) {
            for (auto& buffer : it->second) {
                m_availableBuffers.push_back(std::move(buffer));
            }
            it = m_inUseBuffers.erase(it);
        }
        else {
            ++it;
        }
    }
}

void StagingBufferManager::returnBuffer(Buffer buffer) {
    std::lock_guard<std::mutex> lock(m_mutex);

    bool found = false;
    for (auto it = m_inUseBuffers.begin(); it != m_inUseBuffers.end();) {
        auto& [fence, buffers] = *it;
        auto bufferIt = std::find_if(buffers.begin(), buffers.end(),
            [&buffer](const Buffer& b) { return b.get() == buffer.get(); });

        if (bufferIt != buffers.end()) {
            m_availableBuffers.push_back(std::move(*bufferIt));
            buffers.erase(bufferIt);
            found = true;
            if (buffers.empty()) {
                it = m_inUseBuffers.erase(it);
            }
            else {
                ++it;
            }
            break;
        }
        else {
            ++it;
        }
    }

    if (!found) {
        m_availableBuffers.push_back(std::move(buffer));
    }
}
