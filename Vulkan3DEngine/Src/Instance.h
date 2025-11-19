#pragma once
#include "Prerequisites.h"
#include "VulkanRequirements.h"
#include <memory>
#include <vulkan/vulkan.h>

class DebugMessenger;

class Instance {
public:
    Instance(const VulkanRequirements& requirements);
    ~Instance();

    VkInstance get() const { return m_vkInstance; }
    const VulkanRequirements& getRequirements() const { return m_requirements; }
    uint32_t getApiVersion() const { return m_apiVersion; }

private:
    void validateRequirements();
    void createInstance();

    const VulkanRequirements& m_requirements;
    VkInstance m_vkInstance = VK_NULL_HANDLE;
    uint32_t m_apiVersion = VK_API_VERSION_1_0;
    std::unique_ptr<DebugMessenger> m_debugMessenger;
};
