#pragma once
#include "Prerequisites.h"
#include <stdexcept>
#include <vector>
#include <memory>
#include <vulkan/vulkan.h>

class DebugMessenger;
class ExtensionManager;

class Instance {
public:
    struct Config {
        bool enableValidationLayers;
        bool enableDebugPrintf;
        std::vector<const char*> validationLayers;
        std::vector<const char*> requiredExtensions;
    };

    Instance(const Config& config);
    ~Instance();

    VkInstance get() const { return m_vkInstance; }
    bool validationLayersEnabled() const { return m_config.enableValidationLayers; }
    bool debugPrintfEnabled() const { return m_config.enableDebugPrintf; }

private:
    void createInstance();

    Config m_config;
    VkInstance m_vkInstance = VK_NULL_HANDLE;
    std::unique_ptr<DebugMessenger> m_debugMessenger;
};