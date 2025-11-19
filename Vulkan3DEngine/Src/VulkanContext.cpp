#include "VulkanContext.h"
#include <stdexcept>
#include "Engine.h"
#include "VulkanUtilities.h"

VulkanContext::VulkanContext(const VulkanRequirements& requirements,
    const Window& window,
    Settings& settings)
    : m_requirements(requirements),
    m_settings(settings),
    m_instance(requirements),
    m_surface(m_instance.get(), window),
    m_physicalDevice(m_instance.get(), m_surface.get(), requirements),
    m_logicalDevice(m_physicalDevice, requirements)
{
    const auto& indices = m_physicalDevice.getQueueFamilyIndices();

    // Utwórz command poole
    m_graphicsCommandPool = std::make_unique<CommandPool>(
        m_logicalDevice.get(),
        indices.graphicsFamily.value(),
        m_logicalDevice.getQueueWrapper(QueueType::Graphics),
        QueueType::Graphics
    );

    m_transferCommandPool = std::make_unique<CommandPool>(
        m_logicalDevice.get(),
        indices.transferFamily.value(),
        m_logicalDevice.getQueueWrapper(QueueType::Transfer),
        QueueType::Transfer
    );

    m_computeCommandPool = std::make_unique<CommandPool>(
        m_logicalDevice.get(),
        indices.computeFamily.value(),
        m_logicalDevice.getQueueWrapper(QueueType::Compute),
        QueueType::Compute
    );

    // Zaktualizuj limity sprzętowe w Settings
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
