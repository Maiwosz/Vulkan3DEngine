#include "SynchronizationResourceManager.h"

SynchronizationResourceManager::SynchronizationResourceManager(const LogicalDevice& device)
    : m_device(device) {
}

SynchronizationResourceManager::~SynchronizationResourceManager() {
    // Niszczymy wszystkie semafory
    for (VkSemaphore semaphore : m_freeSemaphores) {
        vkDestroySemaphore(m_device.get(), semaphore, nullptr);
    }
    for (VkSemaphore semaphore : m_usedSemaphores) {
        vkDestroySemaphore(m_device.get(), semaphore, nullptr);
    }

    // Niszczymy wszystkie p這tki
    for (VkFence fence : m_freeFences) {
        vkDestroyFence(m_device.get(), fence, nullptr);
    }
    for (VkFence fence : m_usedFences) {
        vkDestroyFence(m_device.get(), fence, nullptr);
    }
}

// --- Semafory ---
VkSemaphore SynchronizationResourceManager::acquireSemaphore() {
    std::lock_guard<std::mutex> lock(m_semaphoreMutex);

    if (m_freeSemaphores.empty()) {
        // Tworzymy nowy semafor na 蕨danie
        VkSemaphore semaphore = createSemaphore();
        m_usedSemaphores.push_back(semaphore);
        return semaphore;
    }

    // U篡wamy istniej鉍ego semafora
    VkSemaphore semaphore = m_freeSemaphores.back();
    m_freeSemaphores.pop_back();
    m_usedSemaphores.push_back(semaphore);
    return semaphore;
}

void SynchronizationResourceManager::releaseSemaphore(VkSemaphore semaphore) {
    std::lock_guard<std::mutex> lock(m_semaphoreMutex);

    auto it = std::find(m_usedSemaphores.begin(), m_usedSemaphores.end(), semaphore);
    if (it == m_usedSemaphores.end()) {
        throw std::runtime_error("Attempted to release unknown semaphore!");
    }

    m_usedSemaphores.erase(it);
    m_freeSemaphores.push_back(semaphore);
}

// --- P這tki ---
VkFence SynchronizationResourceManager::acquireFence(bool createSignaled) {
    std::lock_guard<std::mutex> lock(m_fenceMutex);

    if (m_freeFences.empty()) {
        // Tworzymy nowy p這tek na 蕨danie
        VkFence fence = createFence(createSignaled);
        m_usedFences.push_back(fence);
        return fence;
    }

    // Resetujemy i u篡wamy istniej鉍ego p這tka
    VkFence fence = m_freeFences.back();
    m_freeFences.pop_back();

    vkResetFences(m_device.get(), 1, &fence);
    m_usedFences.push_back(fence);
    return fence;
}

void SynchronizationResourceManager::releaseFence(VkFence fence) {
    std::lock_guard<std::mutex> lock(m_fenceMutex);

    // Czekamy na zako鎍zenie przed zwolnieniem
    vkWaitForFences(m_device.get(), 1, &fence, VK_TRUE, UINT64_MAX);

    auto it = std::find(m_usedFences.begin(), m_usedFences.end(), fence);
    if (it == m_usedFences.end()) {
        throw std::runtime_error("Attempted to release unknown fence!");
    }

    m_usedFences.erase(it);
    m_freeFences.push_back(fence);
}

// --- Metody pomocnicze ---
VkSemaphore SynchronizationResourceManager::createSemaphore() {
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore semaphore;
    if (vkCreateSemaphore(m_device.get(), &createInfo, nullptr, &semaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create semaphore!");
    }
    return semaphore;
}

VkFence SynchronizationResourceManager::createFence(bool signaled) {
    VkFenceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    VkFence fence;
    if (vkCreateFence(m_device.get(), &createInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fence!");
    }
    return fence;
}