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