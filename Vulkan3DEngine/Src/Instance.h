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

class Instance
{
private:
    Instance();
    ~Instance();
public:
    static Instance* get();
    static void create();
    static void release();
    VkInstance& getVkInstance() { return m_VkInstance; };

    bool validationLayersEnabled() const { return enableValidationLayers; }
    const std::vector<const char*>& getValidationLayers() const { return m_validation_layers; }

    void listAvialableVkExtensions();
private:
    //Instance
    static Instance* m_instance;

    void createInstance();
    std::vector<const char*> getRequiredExtensions();
    bool checkExtensionSupport(const std::vector<const char*>& requiredExtensions);

    //Debug Messenger
    void setupDebugMessenger();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    bool checkValidationLayerSupport();

    //Resources
    VkInstance m_VkInstance;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    
    //Variables
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    const std::vector<const char*> m_validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };
};

