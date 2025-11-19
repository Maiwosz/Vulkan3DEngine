#include "LogicalDevice.h"
#include "Engine.h"
#include <set>

LogicalDevice::LogicalDevice(const PhysicalDevice& physicalDevice,
    const VulkanRequirements& requirements)
    : m_queueFamilyIndices(physicalDevice.getQueueFamilyIndices()) {

    // Przygotuj kolejki
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

    // Pobierz enabled features z PhysicalDevice (już zwalidowane i gotowe do użycia)
    const auto& features10 = physicalDevice.getEnabledFeatures10();
    const auto& features11 = physicalDevice.getEnabledFeatures11();
    const auto& features12 = physicalDevice.getEnabledFeatures12();
    const auto& features13 = physicalDevice.getEnabledFeatures13();
    const auto& extensionFeatures = physicalDevice.getExtensionFeatures();

    // Pobierz listę rozszerzeń do włączenia
    auto extensions = physicalDevice.getDeviceExtensionsToEnable();

    // Zbuduj pNext chain
    // WAŻNE: Używamy const_cast bo potrzebujemy modyfikowalnych pointerów do chainowania
    // ale nie modyfikujemy samych struktur - tylko ich pNext
    VkPhysicalDeviceVulkan11Features* mutableFeatures11 =
        const_cast<VkPhysicalDeviceVulkan11Features*>(&features11);
    VkPhysicalDeviceVulkan12Features* mutableFeatures12 =
        const_cast<VkPhysicalDeviceVulkan12Features*>(&features12);
    VkPhysicalDeviceVulkan13Features* mutableFeatures13 =
        const_cast<VkPhysicalDeviceVulkan13Features*>(&features13);

    mutableFeatures11->pNext = nullptr;
    mutableFeatures12->pNext = mutableFeatures11;
    mutableFeatures13->pNext = mutableFeatures12;

    // Dodaj extension features do chaina
    void* pNext = mutableFeatures13;
    if (!extensionFeatures.structures.empty()) {
        // const_cast jest OK - buildChain nie modyfikuje struktur, tylko chainuje je
        pNext = const_cast<ExtensionFeatureStorage&>(extensionFeatures).buildChain(pNext);
    }

    // Device create info
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &features10;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.pNext = pNext;

    VkResult result = vkCreateDevice(physicalDevice.get(), &createInfo, nullptr, &m_device);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device! Error: " +
            std::to_string(result));
    }

    // Stwórz QueueManager
    m_queueManager = std::make_unique<QueueManager>(m_device, m_queueFamilyIndices);

    SPDLOG_INFO("Logical device created successfully");
}

LogicalDevice::~LogicalDevice() {
    m_queueManager.reset();
    if (m_device) {
        vkDestroyDevice(m_device, nullptr);
    }
}

LogicalDevice::QueueType LogicalDevice::getQueueTypeFromFamilyIndex(uint32_t familyIndex) const {
    if (familyIndex == getQueueFamilyIndex(QueueType::Graphics)) {
        return QueueType::Graphics;
    }
    else if (familyIndex == getQueueFamilyIndex(QueueType::Transfer)) {
        return QueueType::Transfer;
    }
    else if (familyIndex == getQueueFamilyIndex(QueueType::Compute)) {
        return QueueType::Compute;
    }

    return QueueType::Graphics;
}
