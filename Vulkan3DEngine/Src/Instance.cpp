#include "Instance.h"
#include "DebugMessenger.h"
#include "VulkanUtilities.h"

Instance::Instance(const Config& config) : m_config(config) {
    if (m_config.enableValidationLayers) {
        if (!VulkanUtils::checkValidationLayerSupport(m_config.validationLayers)) {
            throw std::runtime_error("Validation layers requested but not available!");
        }
    }

    if (!VulkanUtils::checkSupport(m_config.requiredExtensions)) {
        throw std::runtime_error("Required extensions not supported!");
    }

    createInstance();

    if (m_config.enableValidationLayers) {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        DebugMessenger::populateCreateInfo(createInfo);
        m_debugMessenger = std::make_unique<DebugMessenger>(m_vkInstance, createInfo);
    }
}

Instance::~Instance() {
    m_debugMessenger.reset(); // Destroy debug messenger first
    if (m_vkInstance) {
        vkDestroyInstance(m_vkInstance, nullptr);
    }
}

void Instance::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Application";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Vulkan3DEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_config.requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = m_config.requiredExtensions.data();

    // Add these variables
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    VkValidationFeaturesEXT validationFeatures{};
    void* pNext = nullptr;

    if (m_config.enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_config.validationLayers.size());
        createInfo.ppEnabledLayerNames = m_config.validationLayers.data();

        DebugMessenger::populateCreateInfo(debugCreateInfo);

        // Setup the chain of pNext structures
        if (m_config.enableDebugPrintf) {
            validationFeatures = VulkanUtils::createValidationFeaturesStruct();
            validationFeatures.pNext = &debugCreateInfo;
            pNext = &validationFeatures;
        }
        else {
            pNext = &debugCreateInfo;
        }
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    createInfo.pNext = pNext;

    if (vkCreateInstance(&createInfo, nullptr, &m_vkInstance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
}