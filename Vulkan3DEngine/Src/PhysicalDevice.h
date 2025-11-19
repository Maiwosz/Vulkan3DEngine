#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
#include <map>
#include <memory>
#include "VulkanRequirements.h"

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

// Rezultat walidacji wymagań
struct ValidationResult {
    bool success = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<std::string> info;
    int score = 0;

    void addError(const std::string& msg) {
        errors.push_back(msg);
        success = false;
    }

    void addWarning(const std::string& msg) {
        warnings.push_back(msg);
    }

    void addInfo(const std::string& msg) {
        info.push_back(msg);
    }
};

// Helper do przechowywania extension feature structures
struct ExtensionFeatureStorage {
    std::map<VkStructureType, std::shared_ptr<void>> structures;

    template<typename T>
    T* get(VkStructureType sType) {
        auto it = structures.find(sType);
        if (it != structures.end()) {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }

    template<typename T>
    void add(VkStructureType sType) {
        if (structures.find(sType) == structures.end()) {
            auto ptr = std::make_shared<T>();
            std::memset(ptr.get(), 0, sizeof(T));
            ptr->sType = sType;
            structures[sType] = std::static_pointer_cast<void>(ptr);
        }
    }

    void* buildChain(void* pNext = nullptr);
};

class PhysicalDevice {
public:
    PhysicalDevice(VkInstance instance,
        VkSurfaceKHR surface,
        const VulkanRequirements& requirements);

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

    // Validation result
    const ValidationResult& getValidationResult() const { return m_validationResult; }

    // Query enabled features dla device creation
    const VkPhysicalDeviceFeatures& getEnabledFeatures10() const { return m_enabledFeatures10; }
    const VkPhysicalDeviceVulkan11Features& getEnabledFeatures11() const { return m_enabledFeatures11; }
    const VkPhysicalDeviceVulkan12Features& getEnabledFeatures12() const { return m_enabledFeatures12; }
    const VkPhysicalDeviceVulkan13Features& getEnabledFeatures13() const { return m_enabledFeatures13; }
    const ExtensionFeatureStorage& getExtensionFeatures() const { return m_extensionFeatures; }

    // Get extensions to enable
    std::vector<const char*> getDeviceExtensionsToEnable() const;

    // Dynamic query
    SwapChainSupportDetails querySwapChainSupport() const;
    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

private:
    VkPhysicalDevice m_device = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface;
    const VulkanRequirements& m_requirements;
    ValidationResult m_validationResult;

    // Cached static properties (queried once)
    VkPhysicalDeviceProperties m_deviceProperties;
    VkPhysicalDeviceFeatures m_deviceFeatures;
    VkPhysicalDeviceMemoryProperties m_memoryProperties;
    QueueFamilyIndices m_queueFamilyIndices;
    VkFormat m_depthFormat;
    VkSampleCountFlagBits m_maxMsaaSamples;
    uint32_t m_minImageCount;

    // Available features (queried once)
    VkPhysicalDeviceFeatures m_availableFeatures10;
    VkPhysicalDeviceVulkan11Features m_availableFeatures11;
    VkPhysicalDeviceVulkan12Features m_availableFeatures12;
    VkPhysicalDeviceVulkan13Features m_availableFeatures13;
    ExtensionFeatureStorage m_availableExtensionFeatures;

    // Enabled features (determined by requirements)
    VkPhysicalDeviceFeatures m_enabledFeatures10{};
    VkPhysicalDeviceVulkan11Features m_enabledFeatures11{};
    VkPhysicalDeviceVulkan12Features m_enabledFeatures12{};
    VkPhysicalDeviceVulkan13Features m_enabledFeatures13{};
    ExtensionFeatureStorage m_extensionFeatures;

    // Enabled extensions
    std::vector<std::string> m_enabledExtensions;

    // Helper methods
    void selectBestDevice(VkInstance instance);
    void cacheDeviceProperties();
    void queryAvailableFeatures();
    void determineEnabledFeatures();

    ValidationResult validateDevice(VkPhysicalDevice device);
    int rateDevice(VkPhysicalDevice device);

    static VkFormat findDepthFormat(VkPhysicalDevice device);
    static VkSampleCountFlagBits calculateMaxMsaaSamples(const VkPhysicalDeviceProperties& props);

    // Validation helpers
    bool checkDeviceExtensionSupport(const std::string& extension) const;
    void validateExtensions(ValidationResult& result);
    void validateFeatures(ValidationResult& result);
    void validateQueues(ValidationResult& result);
};
