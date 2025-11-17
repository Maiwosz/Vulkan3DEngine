#pragma once
#include <vulkan/vulkan.h>
#include "PhysicalDevice.h"
#include "QueueManager.h"
#include <memory>

class LogicalDevice {
public:
    // Alias dla kompatybilności wstecznej
    using QueueType = ::QueueType;

    LogicalDevice(const PhysicalDevice& physicalDevice,
        const std::vector<const char*>& deviceExtensions,
        bool enableDebugPrintf);
    ~LogicalDevice();

    VkDevice get() const { return m_device; }

    // Bezpieczny dostęp przez QueueManager
    std::shared_ptr<QueueWrapper> getQueueWrapper(QueueType type) const {
        return m_queueManager->getQueue(type);
    }

    // Legacy compatibility - teraz zwraca raw queue (użyj z ostrożnością!)
    VkQueue getQueue(QueueType type) const {
        return m_queueManager->getRawQueue(type);
    }

    uint32_t getQueueFamilyIndex(QueueType type) const {
        return m_queueManager->getQueueFamilyIndex(type);
    }

    QueueType getQueueTypeFromFamilyIndex(uint32_t familyIndex) const;

    // Dostęp do QueueManager dla zaawansowanych przypadków
    QueueManager& getQueueManager() const { return *m_queueManager; }

    // Sprawdza czy kolejka jest współdzielona
    bool isQueueShared(QueueType type) const {
        return m_queueManager->isQueueShared(type);
    }

private:
    VkDevice m_device;
    QueueFamilyIndices m_queueFamilyIndices;
    std::unique_ptr<QueueManager> m_queueManager;
};
