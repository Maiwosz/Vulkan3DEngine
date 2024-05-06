#pragma once
#include "Prerequisites.h"

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
#include <mutex>

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
    Device();
    ~Device();

    VkPhysicalDevice& getPhysicalDevice() { return m_physicalDevice; };
    VkDevice& get() { return m_device; };
    VkCommandPool& getCommandPool() { return m_commandPool; };
    VkQueue& getGraphicsQueue() { return m_graphicsQueue; };
    VkQueue& getPresentQueue() { return m_presentQueue; };

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
    //Physical Device
    void pickPhysicalDevice();
    //bool isDeviceSuitable(VkPhysicalDevice device);
    int rateDeviceSuitability(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    //Logical Device
    void createLogicalDevice();

    //Command Pool
    void createCommandPool();

    //Depth Buffer
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    //Resources
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    VkCommandPool m_commandPool;

    std::recursive_mutex m_mutex;

    //Variables
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
};

