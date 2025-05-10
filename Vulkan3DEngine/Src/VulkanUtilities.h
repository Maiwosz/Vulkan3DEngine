#pragma once
#include "Prerequisites.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace VulkanUtils {
    bool hasStencilComponent(VkFormat format);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    std::vector<const char*> getRequiredExtensions(bool enableValidationLayers);
    bool checkSupport(const std::vector<const char*>& requiredExtensions);
    bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers);
    void listAvailableExtensions();
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    bool checkDebugPrintfSupport(VkPhysicalDevice device);
    VkValidationFeaturesEXT createValidationFeaturesStruct();
}