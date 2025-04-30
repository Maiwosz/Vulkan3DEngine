#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>
#include <stdexcept>
#include <algorithm>
#include "LogicalDevice.h"

class SynchronizationResourceManager {
public:
    // Konstruktor przyjmuje tylko VkDevice
    explicit SynchronizationResourceManager(const LogicalDevice& device);

    // Destruktor automatycznie czyœci zasoby
    ~SynchronizationResourceManager();

    // Metody zarz¹dzania semaforami
    VkSemaphore acquireSemaphore();
    void releaseSemaphore(VkSemaphore semaphore);

    // Metody zarz¹dzania p³otkami
    VkFence acquireFence(bool createSignaled = false);
    void releaseFence(VkFence fence);

    // Usuwamy konstruktor kopiuj¹cy i operator przypisania
    SynchronizationResourceManager(const SynchronizationResourceManager&) = delete;
    SynchronizationResourceManager& operator=(const SynchronizationResourceManager&) = delete;

private:
    const LogicalDevice& m_device;

    std::vector<VkSemaphore> m_freeSemaphores;
    std::vector<VkSemaphore> m_usedSemaphores;
    std::vector<VkFence> m_freeFences;
    std::vector<VkFence> m_usedFences;

    std::mutex m_semaphoreMutex;
    std::mutex m_fenceMutex;

    VkSemaphore createSemaphore();
    VkFence createFence(bool signaled);
};