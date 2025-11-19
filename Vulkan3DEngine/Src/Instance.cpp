#include "Instance.h"
#include "DebugMessenger.h"
#include "Engine.h"
#include <set>
#include <algorithm>

Instance::Instance(const VulkanRequirements& requirements)
    : m_requirements(requirements) {

    // Waliduj wymagania instancji
    validateRequirements();

    // Jeśli walidacja przeszła, stwórz instancję
    createInstance();

    // Stwórz debug messenger jeśli walidacja włączona
    if (m_requirements.enableValidation) {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        DebugMessenger::populateCreateInfo(createInfo);
        m_debugMessenger = std::make_unique<DebugMessenger>(m_vkInstance, createInfo);
    }
}

Instance::~Instance() {
    m_debugMessenger.reset();
    if (m_vkInstance) {
        vkDestroyInstance(m_vkInstance, nullptr);
    }
}

void Instance::validateRequirements() {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    // 1. Waliduj wersję API
    uint32_t availableVersion = VK_API_VERSION_1_0;
    auto vkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");

    if (vkEnumerateInstanceVersion) {
        if (vkEnumerateInstanceVersion(&availableVersion) != VK_SUCCESS) {
            availableVersion = VK_API_VERSION_1_0;
        }
    }

    if (availableVersion < m_requirements.minimumApiVersion.version) {
        errors.push_back(fmt::format(
            "Vulkan API version {}.{} required, but only {}.{} available",
            VK_VERSION_MAJOR(m_requirements.minimumApiVersion.version),
            VK_VERSION_MINOR(m_requirements.minimumApiVersion.version),
            VK_VERSION_MAJOR(availableVersion),
            VK_VERSION_MINOR(availableVersion)
        ));
    }

    // 2. Waliduj instance extensions
    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> availableExts(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availableExts.data());

    for (const auto& ext : m_requirements.instanceExtensions) {
        bool found = std::any_of(availableExts.begin(), availableExts.end(),
            [&](const VkExtensionProperties& prop) {
                return ext.name == prop.extensionName;
            });

        if (!found) {
            switch (ext.level) {
            case RequirementLevel::Required:
                errors.push_back(fmt::format(
                    "Required instance extension '{}' not available", ext.name));
                break;
            case RequirementLevel::Preferred:
                warnings.push_back(fmt::format(
                    "Preferred instance extension '{}' not available", ext.name));
                break;
            case RequirementLevel::Optional:
                SPDLOG_INFO("Optional instance extension '{}' not available", ext.name);
                break;
            }
        }
    }

    // 3. Waliduj validation layers (jeśli włączone)
    if (m_requirements.enableValidation) {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const auto& layer : m_requirements.validationLayers) {
            bool found = std::any_of(availableLayers.begin(), availableLayers.end(),
                [&](const VkLayerProperties& prop) {
                    return layer.name == prop.layerName;
                });

            if (!found) {
                switch (layer.level) {
                case RequirementLevel::Required:
                    errors.push_back(fmt::format(
                        "Required validation layer '{}' not available", layer.name));
                    break;
                case RequirementLevel::Preferred:
                    warnings.push_back(fmt::format(
                        "Preferred validation layer '{}' not available", layer.name));
                    break;
                case RequirementLevel::Optional:
                    SPDLOG_INFO("Optional validation layer '{}' not available", layer.name);
                    break;
                }
            }
        }
    }

    // Wyświetl ostrzeżenia
    for (const auto& warn : warnings) {
        SPDLOG_WARN("{}", warn);
    }

    // Jeśli są błędy, rzuć wyjątek
    if (!errors.empty()) {
        SPDLOG_ERROR("Instance validation failed:");
        for (const auto& err : errors) {
            SPDLOG_ERROR("  - {}", err);
        }
        throw std::runtime_error("Failed to meet Vulkan instance requirements");
    }
}

void Instance::createInstance() {
    // Wykryj najwyższą dostępną wersję
    uint32_t availableVersion = VK_API_VERSION_1_0;
    auto vkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");

    if (vkEnumerateInstanceVersion) {
        if (vkEnumerateInstanceVersion(&availableVersion) != VK_SUCCESS) {
            availableVersion = VK_API_VERSION_1_0;
        }
    }

    // Użyj najwyższej dostępnej wersji
    m_apiVersion = availableVersion;

    SPDLOG_INFO("Using Vulkan API {}.{}.{} (minimum required: {}.{})",
        VK_VERSION_MAJOR(m_apiVersion),
        VK_VERSION_MINOR(m_apiVersion),
        VK_VERSION_PATCH(m_apiVersion),
        VK_VERSION_MAJOR(m_requirements.minimumApiVersion.version),
        VK_VERSION_MINOR(m_requirements.minimumApiVersion.version));

    // Przygotuj listę rozszerzeń do włączenia (tylko dostępne Required i Preferred)
    std::vector<const char*> extensions;
    std::set<std::string> addedExts; // Zapobieganie duplikatom

    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> availableExts(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availableExts.data());

    for (const auto& ext : m_requirements.instanceExtensions) {
        if (ext.level == RequirementLevel::Required ||
            ext.level == RequirementLevel::Preferred) {

            // Sprawdź czy dostępne
            bool available = std::any_of(availableExts.begin(), availableExts.end(),
                [&](const VkExtensionProperties& prop) {
                    return ext.name == prop.extensionName;
                });

            if (available && addedExts.find(ext.name) == addedExts.end()) {
                extensions.push_back(ext.name.c_str());
                addedExts.insert(ext.name);
            }
        }
    }

    // Przygotuj listę warstw walidacji (tylko dostępne Required i Preferred)
    std::vector<const char*> layers;
    std::set<std::string> addedLayers;

    if (m_requirements.enableValidation) {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const auto& layer : m_requirements.validationLayers) {
            if (layer.level == RequirementLevel::Required ||
                layer.level == RequirementLevel::Preferred) {

                bool available = std::any_of(availableLayers.begin(), availableLayers.end(),
                    [&](const VkLayerProperties& prop) {
                        return layer.name == prop.layerName;
                    });

                if (available && addedLayers.find(layer.name) == addedLayers.end()) {
                    layers.push_back(layer.name.c_str());
                    addedLayers.insert(layer.name);
                }
            }
        }
    }

    // Aplikacja info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Application";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Vulkan3DEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = m_apiVersion;

    // Instance create info
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Warstwy walidacji i debug messenger
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    VkValidationFeaturesEXT validationFeatures{};

    if (m_requirements.enableValidation) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();

        // Debug messenger dla createInstance i destroyInstance
        DebugMessenger::populateCreateInfo(debugCreateInfo);

        // Debug printf setup
        if (m_requirements.enableDebugPrintf) {
            static const VkValidationFeatureEnableEXT enables[] = {
                VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT
            };

            validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            validationFeatures.enabledValidationFeatureCount = 1;
            validationFeatures.pEnabledValidationFeatures = enables;
            validationFeatures.pNext = &debugCreateInfo;

            createInfo.pNext = &validationFeatures;
        }
        else {
            createInfo.pNext = &debugCreateInfo;
        }
    }
    else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    // Stwórz instancję
    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_vkInstance);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance! Error code: " +
            std::to_string(result));
    }

    SPDLOG_INFO("Vulkan instance created successfully");
}
