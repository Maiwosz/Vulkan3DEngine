#pragma once
#include "Instance.h"
#include "PhysicalDevice.h"
#include "LogicalDevice.h"
#include "CommandPool.h"
#include "Surface.h"
#include <functional>

class VulkanContext {
public:
    using SurfaceCreator = std::function<VkSurfaceKHR(VkInstance)>;

    VulkanContext(
        const Instance::Config& instanceConfig,
        const Window& window,
        const std::vector<const char*>& deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }
    );

    const Instance& instance() const { return m_instance; }
    const Surface& surface() const { return m_surface; }
    const PhysicalDevice& physical() const { return m_physicalDevice; }
    const LogicalDevice& logical() const { return m_logicalDevice; }
    CommandPool& graphicsCommandPool() { return *m_graphicsCommandPool; }
    CommandPool& transferCommandPool() { return *m_transferCommandPool; }
    CommandPool& computeCommandPool() { return *m_computeCommandPool; }
    bool debugPrintfEnabled() const { return m_instance.debugPrintfEnabled(); }

private:
    Instance m_instance;
    Surface m_surface;
    PhysicalDevice m_physicalDevice;
    LogicalDevice m_logicalDevice;
    std::unique_ptr<CommandPool> m_graphicsCommandPool;
    std::unique_ptr<CommandPool> m_transferCommandPool;
    std::unique_ptr<CommandPool> m_computeCommandPool;
};