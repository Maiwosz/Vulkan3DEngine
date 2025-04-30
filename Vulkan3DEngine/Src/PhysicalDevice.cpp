#include "PhysicalDevice.h"
#include <stdexcept>
#include <set>
#include <map>
#include <cmath>
#include "VulkanUtilities.h"

QueueFamilyIndices::QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    // Szukaj dedykowanych kolejek
    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        const auto& queueFamily = queueFamilies[i];

        // Graphics
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
        }

        // Present
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) presentFamily = i;

        // Dedicated transfer (bez graphics i compute)
        if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            transferFamily = i;
        }

        // Dedicated compute (bez graphics)
        if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            computeFamily = i;
        }
    }

    // Fallback dla transfer i compute
    if (!transferFamily.has_value()) {
        for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
                transferFamily = i;
                break;
            }
        }
    }

    if (!computeFamily.has_value()) {
        for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                computeFamily = i;
                break;
            }
        }
    }
}

bool QueueFamilyIndices::hasDedicatedTransfer() const {
    return transferFamily.has_value() &&
        transferFamily != graphicsFamily &&
        transferFamily != computeFamily;
}

bool QueueFamilyIndices::hasDedicatedCompute() const {
    return computeFamily.has_value() &&
        computeFamily != graphicsFamily;
}

bool QueueFamilyIndices::isComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value();
}

PhysicalDevice::PhysicalDevice(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& requiredExtensions)
    : m_surface(surface) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::multimap<int, VkPhysicalDevice> candidates;
    for (const auto& device : devices) {
        int score = rateSuitability(device, surface, requiredExtensions);
        if (score > 0) {
            candidates.insert(std::make_pair(score, device));
        }
    }

    if (candidates.empty()) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    m_device = candidates.rbegin()->second;
    m_surface = surface;
    vkGetPhysicalDeviceProperties(m_device, &m_deviceProperties);
    vkGetPhysicalDeviceFeatures(m_device, &m_deviceFeatures);
}

int PhysicalDevice::rateSuitability(VkPhysicalDevice device,
    VkSurfaceKHR surface,
    const std::vector<const char*>& requiredExtensions) {
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(device, &props);
    vkGetPhysicalDeviceFeatures(device, &features);

    int score = 0;

    // Priorytetyzacja dyskretnych GPU
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Wiêksze tekstury = lepsza wydajnoœæ
    if (props.limits.maxImageDimension2D > 0) {
        score += static_cast<int>(std::log2(props.limits.maxImageDimension2D));
    }

    // Wymagane cechy kolejki
    if (!findQueueFamilies(device, surface).isComplete()) {
        return 0;
    }

    // Wsparcie dla wymaganych rozszerzeñ
    if (!checkExtensionSupport(device, requiredExtensions)) {
        return 0;
    }

    // Wsparcie dla swap chain
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
    if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
        return 0;
    }

    // Wymagane funkcje sprzêtowe
    if (!features.samplerAnisotropy) {
        return 0;
    }

    return score;
}

QueueFamilyIndices PhysicalDevice::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    return QueueFamilyIndices(device, surface);
}

SwapChainSupportDetails PhysicalDevice::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkFormat PhysicalDevice::findDepthFormat() const {
    return VulkanUtils::findSupportedFormat(
        m_device,
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool PhysicalDevice::checkExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& extensions) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

VkSampleCountFlagBits PhysicalDevice::getMaxUsableSampleCount() const {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_device, &props);

    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts &
        props.limits.framebufferDepthSampleCounts;

    const std::vector<VkSampleCountFlagBits> flags = {
        VK_SAMPLE_COUNT_64_BIT,
        VK_SAMPLE_COUNT_32_BIT,
        VK_SAMPLE_COUNT_16_BIT,
        VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_4_BIT,
        VK_SAMPLE_COUNT_2_BIT
    };

    for (auto flag : flags) {
        if (counts & flag) return flag;
    }
    return VK_SAMPLE_COUNT_1_BIT;
}

float PhysicalDevice::getMaxAnisotropy() const {
    return m_deviceProperties.limits.maxSamplerAnisotropy;
}

bool PhysicalDevice::isAnisotropySupported() const {
    return m_deviceFeatures.samplerAnisotropy == VK_TRUE;
}

QueueFamilyIndices PhysicalDevice::findQueueFamilies() const {
    return QueueFamilyIndices(m_device, m_surface);
}
