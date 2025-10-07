#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

struct QueueFamilyIndices {
    QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface);

    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> transferFamily;
    std::optional<uint32_t> computeFamily;

    bool isComplete() const;
    bool hasDedicatedTransfer() const;
    bool hasDedicatedCompute() const;
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class PhysicalDevice {
public:
    PhysicalDevice(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME });

    VkPhysicalDevice get() const { return m_device; }

    QueueFamilyIndices findQueueFamilies() const;
    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    VkFormat findDepthFormat() const;
    VkSampleCountFlagBits getMaxUsableSampleCount() const;
    float getMaxAnisotropy() const;
    bool isAnisotropySupported() const;
    uint32_t getMinImageCount() const { return m_minImageCount; }

private:
    VkPhysicalDevice m_device = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface;

    VkPhysicalDeviceProperties m_deviceProperties;
    VkPhysicalDeviceFeatures m_deviceFeatures;

    // Minimalna liczba obrazów w swap chain
    uint32_t m_minImageCount = 2;

    static int rateSuitability(VkPhysicalDevice device,
        VkSurfaceKHR surface,
        const std::vector<const char*>& requiredExtensions);

    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    static bool checkExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& extensions);
};