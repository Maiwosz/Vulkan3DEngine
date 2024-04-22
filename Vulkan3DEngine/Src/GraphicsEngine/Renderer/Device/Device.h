#pragma once
#include "..\..\..\Prerequisites.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <exception>
#include <optional>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <map>
#include <set>
#include <cstdint>
#include <limits>
#include <algorithm>

//Stucts
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Device
{
public:
    Device(Renderer* renderer);
    ~Device();

    VkPhysicalDevice& getPhysicalDevice() { return m_physicalDevice; };
    VkDevice& get() { return m_device; };
    VkSurfaceKHR& getSurface() { return m_surface; };
    VkCommandPool& getCommandPool() { return m_commandPool; };
    VkQueue& getGraphicsQueue() { return m_graphicsQueue; };
    VkQueue& getPresentQueue() { return m_presentQueue; };

    void listAvialableVkExtensions();

    //Swap Chain
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    //Buffers
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    //CommandBuffer
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    
    //Depth Buffer
    VkFormat findDepthFormat();
    bool hasStencilComponent(VkFormat format);

    //Multisampling
    VkSampleCountFlagBits getMaxUsableSampleCount();

private:
    //Instance
    void createInstance();
    std::vector<const char*> getRequiredExtensions();

    //Debug Messenger
    void setupDebugMessenger();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    bool checkValidationLayerSupport();

    //Physical Device
    void pickPhysicalDevice();
    //bool isDeviceSuitable(VkPhysicalDevice device);
    int rateDeviceSuitability(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    //Logical Device
    void createLogicalDevice();

    //Surface
    void createSurface();

    //Command Pool
    void createCommandPool();

    //Depth Buffer
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    //Resources
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    VkSurfaceKHR m_surface;
    VkCommandPool m_commandPool;

    //Pointers and references
    Renderer* m_renderer = nullptr;

    //Variables
    #ifdef NDEBUG
    const bool enableValidationLayers = false;
    #else
    const bool enableValidationLayers = true;
    #endif

    const std::vector<const char*> m_validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
};

