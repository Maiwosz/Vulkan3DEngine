#include "StagingBufferManager.h"
#include <algorithm>
#include <spdlog/spdlog.h>

StagingBufferManager::StagingBufferManager(VmaAllocator allocator, const LogicalDevice& device)
    : m_allocator(allocator), m_device(device) {
}

StagingBufferManager::~StagingBufferManager() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Log remaining buffers before cleanup
    if (!m_availableBuffers.empty() || !m_inUseBuffers.empty()) {
        size_t availableCount = m_availableBuffers.size();
        size_t inUseCount = 0;
        for (const auto& [fence, buffers] : m_inUseBuffers) {
            inUseCount += buffers.size();
        }
        SPDLOG_DEBUG("StagingBufferManager::~StagingBufferManager() - Cleaning up {} available and {} in-use buffers",
            availableCount, inUseCount);
    }

    // Wait for all fences if possible
    for (const auto& [fence, buffers] : m_inUseBuffers) {
        VkResult status = vkGetFenceStatus(m_device.get(), fence);
        if (status == VK_NOT_READY) {
            SPDLOG_WARN("StagingBufferManager::~StagingBufferManager() - Fence still active, waiting...");
            vkWaitForFences(m_device.get(), 1, &fence, VK_TRUE, UINT64_MAX);
        }
    }

    // Clear all buffers
    m_availableBuffers.clear();
    m_inUseBuffers.clear();

    SPDLOG_DEBUG("StagingBufferManager destroyed");
}

Buffer* StagingBufferManager::requestBuffer(VkDeviceSize size, VkFence fence) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Szukaj bufora o wystarczającym rozmiarze w dostępnych
    auto it = std::find_if(m_availableBuffers.begin(), m_availableBuffers.end(),
        [size](const Buffer& b) { return b.getSize() >= size; });

    if (it != m_availableBuffers.end()) {
        // Jeśli znaleziono odpowiedni bufor, przenieś go do używanych
        m_inUseBuffers[fence].push_back(std::move(*it));
        m_availableBuffers.erase(it);

        // Zwróć wskaźnik do bufora w kolekcji m_inUseBuffers
        return &m_inUseBuffers[fence].back();
    }
    else {
        // Jeśli nie znaleziono odpowiedniego bufora, utwórz nowy
        m_inUseBuffers[fence].push_back(Buffer::createStaging(m_allocator, size));

        // Zwróć wskaźnik do nowo utworzonego bufora
        return &m_inUseBuffers[fence].back();
    }
}

void StagingBufferManager::reclaimBuffers() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Przeszukaj wszystkie bufory wg płotków
    auto it = m_inUseBuffers.begin();
    while (it != m_inUseBuffers.end()) {
        VkResult status = vkGetFenceStatus(m_device.get(), it->first);

        // Jeśli płotek sygnalizowany, zwolnij wszystkie powiązane bufory
        if (status == VK_SUCCESS) {
            // Przenieś wszystkie bufory z powrotem do puli dostępnych
            for (auto& buffer : it->second) {
                m_availableBuffers.push_back(std::move(buffer));
            }
            // Usuń wpis dla tego płotka
            it = m_inUseBuffers.erase(it);
        }
        else {
            ++it;
        }
    }
}