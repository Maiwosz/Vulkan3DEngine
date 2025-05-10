#pragma once
#include <vulkan/vulkan.h>
#include "PhysicalDevice.h"

class LogicalDevice {
public:
    enum class QueueType {
        Graphics,
        Present,
        Transfer,
        Compute
    };

    LogicalDevice(const PhysicalDevice& physicalDevice,
        const std::vector<const char*>& deviceExtensions,
        bool enableDebugPrintf);
    ~LogicalDevice();

    VkDevice get() const { return m_device; }
    VkQueue getQueue(QueueType type) const;
    uint32_t getQueueFamilyIndex(QueueType type) const;
private:
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    VkQueue m_transferQueue;
    VkQueue m_computeQueue;
    QueueFamilyIndices m_queueFamilyIndices;
};
