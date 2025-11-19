#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <map>

// Enum do określania poziomu wymagania
enum class RequirementLevel {
    Required,   // Brak = błąd
    Preferred,  // Brak = warning, ale kontynuuj
    Optional    // Brak = tylko info
};

// Bazowa struktura dla wszystkich wymagań
struct Requirement {
    std::string name;
    RequirementLevel level;
    int scoreIfPresent = 0;

    Requirement(std::string n, RequirementLevel l, int score = 0)
        : name(std::move(n)), level(l), scoreIfPresent(score) {
    }
    virtual ~Requirement() = default;
};

// Wymagania dla rozszerzeń instancji
struct InstanceExtension : Requirement {
    using Requirement::Requirement;
};

// Wymagania dla warstw walidacji
struct ValidationLayer : Requirement {
    using Requirement::Requirement;
};

// Wymagania dla rozszerzeń urządzenia
struct DeviceExtension : Requirement {
    using Requirement::Requirement;
};

// Wymagania dla feature'ów Vulkan 1.0
struct DeviceFeature : Requirement {
    VkBool32 VkPhysicalDeviceFeatures::* featurePtr;

    DeviceFeature(std::string n, RequirementLevel l,
        VkBool32 VkPhysicalDeviceFeatures::* ptr, int score = 0)
        : Requirement(std::move(n), l, score), featurePtr(ptr) {
    }
};

// Wymagania dla feature'ów Vulkan 1.1
struct DeviceFeature11 : Requirement {
    VkBool32 VkPhysicalDeviceVulkan11Features::* featurePtr;

    DeviceFeature11(std::string n, RequirementLevel l,
        VkBool32 VkPhysicalDeviceVulkan11Features::* ptr, int score = 0)
        : Requirement(std::move(n), l, score), featurePtr(ptr) {
    }
};

// Wymagania dla feature'ów Vulkan 1.2
struct DeviceFeature12 : Requirement {
    VkBool32 VkPhysicalDeviceVulkan12Features::* featurePtr;

    DeviceFeature12(std::string n, RequirementLevel l,
        VkBool32 VkPhysicalDeviceVulkan12Features::* ptr, int score = 0)
        : Requirement(std::move(n), l, score), featurePtr(ptr) {
    }
};

// Wymagania dla feature'ów Vulkan 1.3
struct DeviceFeature13 : Requirement {
    VkBool32 VkPhysicalDeviceVulkan13Features::* featurePtr;

    DeviceFeature13(std::string n, RequirementLevel l,
        VkBool32 VkPhysicalDeviceVulkan13Features::* ptr, int score = 0)
        : Requirement(std::move(n), l, score), featurePtr(ptr) {
    }
};

// ==================== Extension Features ====================

// Wymaganie dla pojedynczego extension feature
struct ExtensionFeatureRequirement : Requirement {
    VkStructureType structType;
    std::function<void(void*)> enableFeature;      // Funkcja włączająca feature
    std::function<bool(const void*)> checkFeature; // Funkcja sprawdzająca dostępność

    ExtensionFeatureRequirement(
        std::string name,
        RequirementLevel level,
        VkStructureType sType,
        std::function<void(void*)> enableFn,
        std::function<bool(const void*)> checkFn,
        int score = 0)
        : Requirement(std::move(name), level, score)
        , structType(sType)
        , enableFeature(std::move(enableFn))
        , checkFeature(std::move(checkFn)) {
    }
};

// ==================== Wymagania dla kolejek ====================

struct QueueRequirement : Requirement {
    VkQueueFlags flags;
    bool dedicatedPreferred;

    QueueRequirement(std::string n, VkQueueFlags f, bool dedicated = false, int score = 0)
        : Requirement(std::move(n), RequirementLevel::Required, score),
        flags(f), dedicatedPreferred(dedicated) {
    }
};

// Wymagania wersji API
struct ApiVersion {
    uint32_t version;
    std::string description;
};

// ==================== GŁÓWNA STRUKTURA - CZYSTA "LISTA ŻYCZEŃ" ====================

struct VulkanRequirements {
    ApiVersion minimumApiVersion;

    std::vector<InstanceExtension> instanceExtensions;
    std::vector<ValidationLayer> validationLayers;
    std::vector<DeviceExtension> deviceExtensions;
    std::vector<DeviceFeature> deviceFeatures;
    std::vector<DeviceFeature11> deviceFeatures11;
    std::vector<DeviceFeature12> deviceFeatures12;
    std::vector<DeviceFeature13> deviceFeatures13;
    std::vector<ExtensionFeatureRequirement> extensionFeatures;
    std::vector<QueueRequirement> queueRequirements;

    // Flagi konfiguracyjne
    bool enableValidation = false;
    bool enableDebugPrintf = false;

    // ==================== Helper methods ====================

    void requireInstanceExtension(const char* name, int score = 0) {
        instanceExtensions.emplace_back(name, RequirementLevel::Required, score);
    }

    void preferInstanceExtension(const char* name, int score = 50) {
        instanceExtensions.emplace_back(name, RequirementLevel::Preferred, score);
    }

    void requireDeviceExtension(const char* name, int score = 0) {
        deviceExtensions.emplace_back(name, RequirementLevel::Required, score);
    }

    void preferDeviceExtension(const char* name, int score = 50) {
        deviceExtensions.emplace_back(name, RequirementLevel::Preferred, score);
    }

    void requireFeature(const char* name, VkBool32 VkPhysicalDeviceFeatures::* ptr, int score = 0) {
        deviceFeatures.emplace_back(name, RequirementLevel::Required, ptr, score);
    }

    void preferFeature(const char* name, VkBool32 VkPhysicalDeviceFeatures::* ptr, int score = 50) {
        deviceFeatures.emplace_back(name, RequirementLevel::Preferred, ptr, score);
    }

    void requireFeature11(const char* name, VkBool32 VkPhysicalDeviceVulkan11Features::* ptr, int score = 0) {
        deviceFeatures11.emplace_back(name, RequirementLevel::Required, ptr, score);
    }

    void preferFeature11(const char* name, VkBool32 VkPhysicalDeviceVulkan11Features::* ptr, int score = 50) {
        deviceFeatures11.emplace_back(name, RequirementLevel::Preferred, ptr, score);
    }

    void requireFeature12(const char* name, VkBool32 VkPhysicalDeviceVulkan12Features::* ptr, int score = 0) {
        deviceFeatures12.emplace_back(name, RequirementLevel::Required, ptr, score);
    }

    void preferFeature12(const char* name, VkBool32 VkPhysicalDeviceVulkan12Features::* ptr, int score = 50) {
        deviceFeatures12.emplace_back(name, RequirementLevel::Preferred, ptr, score);
    }

    void requireFeature13(const char* name, VkBool32 VkPhysicalDeviceVulkan13Features::* ptr, int score = 0) {
        deviceFeatures13.emplace_back(name, RequirementLevel::Required, ptr, score);
    }

    void preferFeature13(const char* name, VkBool32 VkPhysicalDeviceVulkan13Features::* ptr, int score = 50) {
        deviceFeatures13.emplace_back(name, RequirementLevel::Preferred, ptr, score);
    }

    void requireQueue(const char* name, VkQueueFlags flags, bool dedicated = false, int score = 0) {
        queueRequirements.emplace_back(name, flags, dedicated, score);
    }

    // ==================== Extension Features Helper ====================

    template<typename FeatureStruct, typename MemberType>
    void requireExtensionFeature(
        const char* name,
        VkStructureType sType,
        MemberType FeatureStruct::* featurePtr,
        int score = 0) {
        addExtensionFeature(name, RequirementLevel::Required, sType, featurePtr, score);
    }

    template<typename FeatureStruct, typename MemberType>
    void preferExtensionFeature(
        const char* name,
        VkStructureType sType,
        MemberType FeatureStruct::* featurePtr,
        int score = 50) {
        addExtensionFeature(name, RequirementLevel::Preferred, sType, featurePtr, score);
    }

    template<typename FeatureStruct, typename MemberType>
    void optionalExtensionFeature(
        const char* name,
        VkStructureType sType,
        MemberType FeatureStruct::* featurePtr,
        int score = 0) {
        addExtensionFeature(name, RequirementLevel::Optional, sType, featurePtr, score);
    }

private:
    template<typename FeatureStruct, typename MemberType>
    void addExtensionFeature(
        const char* name,
        RequirementLevel level,
        VkStructureType sType,
        MemberType FeatureStruct::* featurePtr,
        int score) {

        auto enableFn = [featurePtr](void* structPtr) {
            auto* features = static_cast<FeatureStruct*>(structPtr);
            features->*featurePtr = VK_TRUE;
            };

        auto checkFn = [featurePtr](const void* structPtr) -> bool {
            const auto* features = static_cast<const FeatureStruct*>(structPtr);
            return (features->*featurePtr) == VK_TRUE;
            };

        extensionFeatures.emplace_back(
            name,
            level,
            sType,
            std::move(enableFn),
            std::move(checkFn),
            score
        );
    }
};
