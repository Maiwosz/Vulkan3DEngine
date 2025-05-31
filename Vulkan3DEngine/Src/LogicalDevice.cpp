#include "LogicalDevice.h"
#include <stdexcept>
#include <set>

LogicalDevice::LogicalDevice(const PhysicalDevice& physicalDevice,
    const std::vector<const char*>& deviceExtensions,
    bool enableDebugPrintf)
    : m_queueFamilyIndices(physicalDevice.findQueueFamilies())
{
    std::set<uint32_t> uniqueQueueFamilies = {
        m_queueFamilyIndices.graphicsFamily.value(),
        m_queueFamilyIndices.presentFamily.value(),
        m_queueFamilyIndices.transferFamily.value(),
        m_queueFamilyIndices.computeFamily.value()
    };

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;

    std::vector<const char*> extensions = deviceExtensions;
    if (enableDebugPrintf) {
        extensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (vkCreateDevice(physicalDevice.get(), &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(m_device, m_queueFamilyIndices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_queueFamilyIndices.presentFamily.value(), 0, &m_presentQueue);
    if (m_queueFamilyIndices.transferFamily.has_value()) {
        vkGetDeviceQueue(m_device, m_queueFamilyIndices.transferFamily.value(), 0, &m_transferQueue);
    }
    if (m_queueFamilyIndices.computeFamily.has_value()) {
        vkGetDeviceQueue(m_device, m_queueFamilyIndices.computeFamily.value(), 0, &m_computeQueue);
    }
}

LogicalDevice::~LogicalDevice() {
    vkDestroyDevice(m_device, nullptr);
}

VkQueue LogicalDevice::getQueue(QueueType type) const {
    switch (type) {
    case QueueType::Graphics: return m_graphicsQueue;
    case QueueType::Present: return m_presentQueue;
    case QueueType::Transfer: return m_transferQueue;
    case QueueType::Compute: return m_computeQueue;
    default: throw std::invalid_argument("Unknown queue type");
    }
}

uint32_t LogicalDevice::getQueueFamilyIndex(QueueType type) const {
    switch (type) {
    case QueueType::Graphics:
        return m_queueFamilyIndices.graphicsFamily.value();
    case QueueType::Present:
        return m_queueFamilyIndices.presentFamily.value();
    case QueueType::Transfer:
        return m_queueFamilyIndices.transferFamily.value();
    case QueueType::Compute:
        return m_queueFamilyIndices.computeFamily.value();
    default:
        throw std::invalid_argument("Invalid QueueType");
    }
}

LogicalDevice::QueueType LogicalDevice::getQueueTypeFromFamilyIndex(uint32_t familyIndex) const {
    // This is a simplified mapping - you might need to adjust based on your actual queue family setup
    if (familyIndex == getQueueFamilyIndex(LogicalDevice::QueueType::Graphics)) {
        return LogicalDevice::QueueType::Graphics;
    }
    else if (familyIndex == getQueueFamilyIndex(LogicalDevice::QueueType::Transfer)) {
        return LogicalDevice::QueueType::Transfer;
    }
    else if (familyIndex == getQueueFamilyIndex(LogicalDevice::QueueType::Compute)) {
        return LogicalDevice::QueueType::Compute;
    }

    // Default to Graphics if we can't determine
    return LogicalDevice::QueueType::Graphics;
}