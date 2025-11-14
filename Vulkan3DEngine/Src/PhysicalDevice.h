#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

struct QueueFamilyIndices {
    QueueFamilyIndices() = default;
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
    // Struktura opisująca wymaganą funkcję sprzętową
    struct RequiredFeature {
        const char* name;
        VkBool32 VkPhysicalDeviceFeatures::* feature;
    };

    // Struktura opisująca opcjonalną funkcję z punktacją
    struct OptionalFeature {
        const char* name;
        VkBool32 VkPhysicalDeviceFeatures::* feature;
        int score;
    };

    PhysicalDevice(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME });

    VkPhysicalDevice get() const { return m_device; }

    // Cached properties
    const QueueFamilyIndices& getQueueFamilyIndices() const { return m_queueFamilyIndices; }
    const VkPhysicalDeviceProperties& getProperties() const { return m_deviceProperties; }
    const VkPhysicalDeviceFeatures& getFeatures() const { return m_deviceFeatures; }
    const VkPhysicalDeviceMemoryProperties& getMemoryProperties() const { return m_memoryProperties; }
    VkFormat getDepthFormat() const { return m_depthFormat; }
    VkSampleCountFlagBits getMaxUsableSampleCount() const { return m_maxMsaaSamples; }
    float getMaxAnisotropy() const { return m_deviceProperties.limits.maxSamplerAnisotropy; }
    bool isAnisotropySupported() const { return m_deviceFeatures.samplerAnisotropy == VK_TRUE; }
    uint32_t getMinImageCount() const { return m_minImageCount; }

    // Backward compatibility
    QueueFamilyIndices findQueueFamilies() const { return m_queueFamilyIndices; }

    // Dynamic query
    SwapChainSupportDetails querySwapChainSupport() const;
    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

private:
    VkPhysicalDevice m_device = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface;

    // Cached static properties
    VkPhysicalDeviceProperties m_deviceProperties;
    VkPhysicalDeviceFeatures m_deviceFeatures;
    VkPhysicalDeviceMemoryProperties m_memoryProperties;
    QueueFamilyIndices m_queueFamilyIndices;
    VkFormat m_depthFormat;
    VkSampleCountFlagBits m_maxMsaaSamples;
    uint32_t m_minImageCount;

    // Definicje wymaganych i opcjonalnych funkcji
    static constexpr RequiredFeature REQUIRED_FEATURES[] = {
        {"samplerAnisotropy", &VkPhysicalDeviceFeatures::samplerAnisotropy}
    };

    static constexpr OptionalFeature OPTIONAL_FEATURES[] = {
        {"sampleRateShading", &VkPhysicalDeviceFeatures::sampleRateShading, 50},
        {"geometryShader", &VkPhysicalDeviceFeatures::geometryShader, 100},
        {"tessellationShader", &VkPhysicalDeviceFeatures::tessellationShader, 100},
        {"multiDrawIndirect", &VkPhysicalDeviceFeatures::multiDrawIndirect, 75},
        {"fillModeNonSolid", &VkPhysicalDeviceFeatures::fillModeNonSolid, 25},
        {"wideLines", &VkPhysicalDeviceFeatures::wideLines, 10},
        {"largePoints", &VkPhysicalDeviceFeatures::largePoints, 10},
        {"textureCompressionBC", &VkPhysicalDeviceFeatures::textureCompressionBC, 30}
    };

    // Helper methods
    void cacheDeviceProperties();
    int rateSuitability(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& requiredExtensions);
    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    static bool checkExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& extensions);
    static VkFormat findDepthFormat(VkPhysicalDevice device);
    static VkSampleCountFlagBits calculateMaxMsaaSamples(const VkPhysicalDeviceProperties& props);

    // Nowe pomocnicze metody dla feature'ów
    static bool checkRequiredFeatures(const VkPhysicalDeviceFeatures& features);
    static int scoreOptionalFeatures(const VkPhysicalDeviceFeatures& features);
};
