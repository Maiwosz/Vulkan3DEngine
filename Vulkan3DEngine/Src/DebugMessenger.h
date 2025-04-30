#pragma once
#include <vulkan/vulkan.h>

class DebugMessenger {
public:
    DebugMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    ~DebugMessenger();

    static void populateCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;
};