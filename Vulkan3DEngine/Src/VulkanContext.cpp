#include "VulkanContext.h"
#include <stdexcept>
#include "Engine.h"
#include "VulkanUtilities.h"

VulkanContext::VulkanContext(
    const Instance::Config& instanceConfig,
    const Window& window,
    Settings& settings,
    const std::vector<const char*>& deviceExtensions)
    : m_settings(settings),
    m_instance(instanceConfig),
    m_surface(m_instance.get(), window),
    m_physicalDevice(m_instance.get(), m_surface.get(), deviceExtensions),
    m_logicalDevice(m_physicalDevice, deviceExtensions, instanceConfig.enableDebugPrintf)
{
    const auto& indices = m_physicalDevice.getQueueFamilyIndices();

    m_graphicsCommandPool = std::make_unique<CommandPool>(
        m_logicalDevice.get(),
        indices.graphicsFamily.value(),
        m_logicalDevice.getQueue(LogicalDevice::QueueType::Graphics)
    );

    m_transferCommandPool = std::make_unique<CommandPool>(
        m_logicalDevice.get(),
        indices.transferFamily.value(),
        m_logicalDevice.getQueue(LogicalDevice::QueueType::Transfer)
    );

    m_computeCommandPool = std::make_unique<CommandPool>(
        m_logicalDevice.get(),
        indices.computeFamily.value(),
        m_logicalDevice.getQueue(LogicalDevice::QueueType::Compute)
    );

    // Pobierz limity sprzętowe z cache i zaktualizuj Settings
    VkSampleCountFlagBits vkMaxMsaa = m_physicalDevice.getMaxUsableSampleCount();
    Settings::MsaaSampleCount maxMsaa = static_cast<Settings::MsaaSampleCount>(vkMaxMsaa);
    float maxAnisotropy = m_physicalDevice.getMaxAnisotropy();
    bool anisotropySupported = m_physicalDevice.isAnisotropySupported();

    Settings::HardwareLimits limits = {
        maxMsaa,
        maxAnisotropy,
        anisotropySupported
    };

    m_settings.setHardwareLimits(limits);
}
