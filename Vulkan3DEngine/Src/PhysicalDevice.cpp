#include "PhysicalDevice.h"
#include "Engine.h"
#include "VulkanUtilities.h"
#include <map>
#include <algorithm>

// ==================== QueueFamilyIndices ====================

QueueFamilyIndices::QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        const auto& queueFamily = queueFamilies[i];

        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) presentFamily = i;

        // Dedicated transfer
        if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            transferFamily = i;
        }

        // Dedicated compute
        if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            computeFamily = i;
        }
    }

    // Fallback
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
    return graphicsFamily.has_value() &&
        presentFamily.has_value() &&
        transferFamily.has_value() &&
        computeFamily.has_value();
}

// ==================== ExtensionFeatureStorage ====================

void* ExtensionFeatureStorage::buildChain(void* pNext) {
    for (auto& [sType, ptr] : structures) {
        auto* base = static_cast<VkBaseOutStructure*>(ptr.get());
        base->pNext = static_cast<VkBaseOutStructure*>(pNext);
        pNext = ptr.get();
    }
    return pNext;
}

// ==================== PhysicalDevice ====================

PhysicalDevice::PhysicalDevice(VkInstance instance,
    VkSurfaceKHR surface,
    const VulkanRequirements& requirements)
    : m_surface(surface), m_requirements(requirements) {

    selectBestDevice(instance);
    cacheDeviceProperties();
    queryAvailableFeatures();
    determineEnabledFeatures();

    // Wyświetl rezultaty walidacji
    if (!m_validationResult.success) {
        SPDLOG_ERROR("Selected device doesn't fully meet requirements:");
        for (const auto& err : m_validationResult.errors) {
            SPDLOG_ERROR("  - {}", err);
        }
        throw std::runtime_error("Selected GPU doesn't meet requirements");
    }

    for (const auto& warn : m_validationResult.warnings) {
        SPDLOG_WARN("{}", warn);
    }

    for (const auto& info : m_validationResult.info) {
        SPDLOG_INFO("{}", info);
    }

    SPDLOG_INFO("Selected GPU: {} (Score: {})",
        m_deviceProperties.deviceName,
        m_validationResult.score);
}

void PhysicalDevice::selectBestDevice(VkInstance instance) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Oceń wszystkie urządzenia
    std::multimap<int, VkPhysicalDevice> candidates;
    for (const auto& device : devices) {
        int score = rateDevice(device);
        if (score > 0) {
            candidates.insert(std::make_pair(score, device));
        }
    }

    if (candidates.empty()) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    // Wybierz najlepsze urządzenie
    m_device = candidates.rbegin()->second;

    // Zachowaj wynik walidacji dla najlepszego urządzenia
    m_validationResult = validateDevice(m_device);
}

int PhysicalDevice::rateDevice(VkPhysicalDevice device) {
    ValidationResult result = validateDevice(device);

    // Jeśli nie spełnia wymagań, zwróć 0
    if (!result.success) {
        return 0;
    }

    int score = result.score;

    // Dodatkowe punkty za typ urządzenia
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Punkty za wielkość tekstur
    if (props.limits.maxImageDimension2D > 0) {
        score += static_cast<int>(std::log2(props.limits.maxImageDimension2D));
    }

    // Sprawdź swap chain
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, m_surface);
    if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
        return 0;
    }

    return score;
}

ValidationResult PhysicalDevice::validateDevice(VkPhysicalDevice device) {
    ValidationResult result;

    // Tymczasowo ustaw device do sprawdzenia
    VkPhysicalDevice originalDevice = m_device;
    m_device = device;

    validateExtensions(result);

    // Query features tylko dla tego urządzenia
    VkPhysicalDeviceFeatures tempFeatures10{};
    VkPhysicalDeviceVulkan11Features tempFeatures11{};
    VkPhysicalDeviceVulkan12Features tempFeatures12{};
    VkPhysicalDeviceVulkan13Features tempFeatures13{};

    tempFeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    tempFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    tempFeatures12.pNext = &tempFeatures11;
    tempFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    tempFeatures13.pNext = &tempFeatures12;

    // Query extension features jeśli są wymagane
    ExtensionFeatureStorage tempExtFeatures;
    void* pNext = &tempFeatures13;

    if (!m_requirements.extensionFeatures.empty()) {
        // Przygotuj struktury dla wszystkich extension features
        for (const auto& feat : m_requirements.extensionFeatures) {
            switch (feat.structType) {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT:
                tempExtFeatures.add<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>(feat.structType);
                break;
                // Dodaj inne typy extension features tutaj
            }
        }
        pNext = tempExtFeatures.buildChain(pNext);
    }

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = pNext;

    vkGetPhysicalDeviceFeatures2(device, &features2);
    tempFeatures10 = features2.features;

    // Waliduj features używając temporary queried features
    for (const auto& feat : m_requirements.deviceFeatures) {
        bool supported = tempFeatures10.*(feat.featurePtr);
        if (!supported) {
            switch (feat.level) {
            case RequirementLevel::Required:
                result.addError(fmt::format("Required device feature '{}' not available", feat.name));
                break;
            case RequirementLevel::Preferred:
                result.addWarning(fmt::format("Preferred device feature '{}' not available", feat.name));
                break;
            case RequirementLevel::Optional:
                result.addInfo(fmt::format("Optional device feature '{}' not available", feat.name));
                break;
            }
        }
        else {
            result.score += feat.scoreIfPresent;
        }
    }

    for (const auto& feat : m_requirements.deviceFeatures11) {
        bool supported = tempFeatures11.*(feat.featurePtr);
        if (!supported) {
            switch (feat.level) {
            case RequirementLevel::Required:
                result.addError(fmt::format("Required feature 1.1 '{}' not available", feat.name));
                break;
            case RequirementLevel::Preferred:
                result.addWarning(fmt::format("Preferred feature 1.1 '{}' not available", feat.name));
                break;
            case RequirementLevel::Optional:
                result.addInfo(fmt::format("Optional feature 1.1 '{}' not available", feat.name));
                break;
            }
        }
        else {
            result.score += feat.scoreIfPresent;
        }
    }

    for (const auto& feat : m_requirements.deviceFeatures12) {
        bool supported = tempFeatures12.*(feat.featurePtr);
        if (!supported) {
            switch (feat.level) {
            case RequirementLevel::Required:
                result.addError(fmt::format("Required feature 1.2 '{}' not available", feat.name));
                break;
            case RequirementLevel::Preferred:
                result.addWarning(fmt::format("Preferred feature 1.2 '{}' not available", feat.name));
                break;
            case RequirementLevel::Optional:
                result.addInfo(fmt::format("Optional feature 1.2 '{}' not available", feat.name));
                break;
            }
        }
        else {
            result.score += feat.scoreIfPresent;
        }
    }

    for (const auto& feat : m_requirements.deviceFeatures13) {
        bool supported = tempFeatures13.*(feat.featurePtr);
        if (!supported) {
            switch (feat.level) {
            case RequirementLevel::Required:
                result.addError(fmt::format("Required feature 1.3 '{}' not available", feat.name));
                break;
            case RequirementLevel::Preferred:
                result.addWarning(fmt::format("Preferred feature 1.3 '{}' not available", feat.name));
                break;
            case RequirementLevel::Optional:
                result.addInfo(fmt::format("Optional feature 1.3 '{}' not available", feat.name));
                break;
            }
        }
        else {
            result.score += feat.scoreIfPresent;
        }
    }

    // Extension features
    for (const auto& feat : m_requirements.extensionFeatures) {
        void* structPtr = nullptr;
        switch (feat.structType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT:
            structPtr = tempExtFeatures.get<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>(feat.structType);
            break;
        }

        if (structPtr) {
            bool supported = feat.checkFeature(structPtr);
            if (!supported) {
                switch (feat.level) {
                case RequirementLevel::Required:
                    result.addError(fmt::format("Required extension feature '{}' not available", feat.name));
                    break;
                case RequirementLevel::Preferred:
                    result.addWarning(fmt::format("Preferred extension feature '{}' not available", feat.name));
                    break;
                case RequirementLevel::Optional:
                    result.addInfo(fmt::format("Optional extension feature '{}' not available", feat.name));
                    break;
                }
            }
            else {
                result.score += feat.scoreIfPresent;
            }
        }
    }

    validateQueues(result);

    // Przywróć oryginalny device
    m_device = originalDevice;

    return result;
}

bool PhysicalDevice::checkDeviceExtensionSupport(const std::string& extension) const {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(m_device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(m_device, nullptr, &count, available.data());

    return std::any_of(available.begin(), available.end(),
        [&](const VkExtensionProperties& prop) {
            return extension == prop.extensionName;
        });
}

void PhysicalDevice::validateExtensions(ValidationResult& result) {
    for (const auto& ext : m_requirements.deviceExtensions) {
        bool supported = checkDeviceExtensionSupport(ext.name);

        if (!supported) {
            switch (ext.level) {
            case RequirementLevel::Required:
                result.addError(fmt::format("Required device extension '{}' not available", ext.name));
                break;
            case RequirementLevel::Preferred:
                result.addWarning(fmt::format("Preferred device extension '{}' not available", ext.name));
                break;
            case RequirementLevel::Optional:
                result.addInfo(fmt::format("Optional device extension '{}' not available", ext.name));
                break;
            }
        }
        else {
            result.score += ext.scoreIfPresent;
            // Zapamiętaj extension do włączenia później
            if (std::find(m_enabledExtensions.begin(), m_enabledExtensions.end(), ext.name) == m_enabledExtensions.end()) {
                m_enabledExtensions.push_back(ext.name);
            }
        }
    }
}

void PhysicalDevice::validateQueues(ValidationResult& result) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_device, &queueFamilyCount, queueFamilies.data());

    for (const auto& req : m_requirements.queueRequirements) {
        bool found = false;
        bool foundDedicated = false;

        for (const auto& qf : queueFamilies) {
            if ((qf.queueFlags & req.flags) == req.flags) {
                found = true;

                if (req.dedicatedPreferred) {
                    VkQueueFlags otherFlags = qf.queueFlags & ~req.flags;
                    if (otherFlags == 0 || otherFlags == VK_QUEUE_TRANSFER_BIT) {
                        foundDedicated = true;
                        result.score += req.scoreIfPresent;
                    }
                }
                break;
            }
        }

        if (!found) {
            result.addError(fmt::format("Required queue family '{}' not available", req.name));
        }
        else if (req.dedicatedPreferred && !foundDedicated) {
            result.addInfo(fmt::format("Dedicated queue '{}' preferred but not available", req.name));
        }
    }
}

void PhysicalDevice::cacheDeviceProperties() {
    vkGetPhysicalDeviceProperties(m_device, &m_deviceProperties);
    vkGetPhysicalDeviceFeatures(m_device, &m_deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(m_device, &m_memoryProperties);

    m_queueFamilyIndices = QueueFamilyIndices(m_device, m_surface);

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device, m_surface, &surfaceCapabilities);
    m_minImageCount = surfaceCapabilities.minImageCount;

    m_depthFormat = findDepthFormat(m_device);
    m_maxMsaaSamples = calculateMaxMsaaSamples(m_deviceProperties);
}

void PhysicalDevice::queryAvailableFeatures() {
    m_availableFeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    m_availableFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    m_availableFeatures12.pNext = &m_availableFeatures11;
    m_availableFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    m_availableFeatures13.pNext = &m_availableFeatures12;

    // Przygotuj extension features
    void* pNext = &m_availableFeatures13;

    if (!m_requirements.extensionFeatures.empty()) {
        for (const auto& feat : m_requirements.extensionFeatures) {
            switch (feat.structType) {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT:
                m_availableExtensionFeatures.add<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>(feat.structType);
                break;
            }
        }
        pNext = m_availableExtensionFeatures.buildChain(pNext);
    }

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = pNext;

    vkGetPhysicalDeviceFeatures2(m_device, &features2);
    m_availableFeatures10 = features2.features;
}

// Tylko fragment funkcji determineEnabledFeatures() - zastąp odpowiednią sekcję

void PhysicalDevice::determineEnabledFeatures() {
    // Włącz tylko te features, które są dostępne i wymagane/preferowane
    for (const auto& feat : m_requirements.deviceFeatures) {
        if ((feat.level == RequirementLevel::Required || feat.level == RequirementLevel::Preferred) &&
            m_availableFeatures10.*(feat.featurePtr)) {
            m_enabledFeatures10.*(feat.featurePtr) = VK_TRUE;
        }
    }

    // Inicjalizuj struktury
    m_enabledFeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    m_enabledFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    m_enabledFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    for (const auto& feat : m_requirements.deviceFeatures11) {
        if ((feat.level == RequirementLevel::Required || feat.level == RequirementLevel::Preferred) &&
            m_availableFeatures11.*(feat.featurePtr)) {
            m_enabledFeatures11.*(feat.featurePtr) = VK_TRUE;
        }
    }

    for (const auto& feat : m_requirements.deviceFeatures12) {
        if ((feat.level == RequirementLevel::Required || feat.level == RequirementLevel::Preferred) &&
            m_availableFeatures12.*(feat.featurePtr)) {
            m_enabledFeatures12.*(feat.featurePtr) = VK_TRUE;
        }
    }

    for (const auto& feat : m_requirements.deviceFeatures13) {
        if ((feat.level == RequirementLevel::Required || feat.level == RequirementLevel::Preferred) &&
            m_availableFeatures13.*(feat.featurePtr)) {
            m_enabledFeatures13.*(feat.featurePtr) = VK_TRUE;
        }
    }

    // Extension features - POPRAWIONE: Sprawdź dostępność PRZED włączeniem
    for (const auto& feat : m_requirements.extensionFeatures) {
        if (feat.level == RequirementLevel::Required || feat.level == RequirementLevel::Preferred) {
            // Sprawdź czy feature jest dostępne
            void* availableStructPtr = nullptr;
            switch (feat.structType) {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT:
                availableStructPtr = m_availableExtensionFeatures.get<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>(feat.structType);
                break;
            }

            // Włącz tylko jeśli jest dostępne
            if (availableStructPtr && feat.checkFeature(availableStructPtr)) {
                m_extensionFeatures.add<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>(feat.structType);
                auto* extFeat = m_extensionFeatures.get<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>(feat.structType);
                if (extFeat) {
                    feat.enableFeature(extFeat);
                }
            }
        }
    }
}

std::vector<const char*> PhysicalDevice::getDeviceExtensionsToEnable() const {
    std::vector<const char*> extensions;
    for (const auto& ext : m_enabledExtensions) {
        extensions.push_back(ext.c_str());
    }
    return extensions;
}

SwapChainSupportDetails PhysicalDevice::querySwapChainSupport() const {
    return querySwapChainSupport(m_device, m_surface);
}

SwapChainSupportDetails PhysicalDevice::querySwapChainSupport(
    VkPhysicalDevice device, VkSurfaceKHR surface) {

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
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
            details.presentModes.data());
    }

    return details;
}

VkFormat PhysicalDevice::findDepthFormat(VkPhysicalDevice device) {
    return VulkanUtils::findSupportedFormat(
        device,
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkSampleCountFlagBits PhysicalDevice::calculateMaxMsaaSamples(
    const VkPhysicalDeviceProperties& props) {

    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts &
        props.limits.framebufferDepthSampleCounts;

    const std::vector<VkSampleCountFlagBits> flags = {
        VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT,
        VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_2_BIT
    };

    for (auto flag : flags) {
        if (counts & flag) return flag;
    }
    return VK_SAMPLE_COUNT_1_BIT;
}
