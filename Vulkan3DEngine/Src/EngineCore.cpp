#include "EngineCore.h"
#include "VulkanUtilities.h"
#include <stdexcept>
#include "Engine.h"
#include "GraphicsTypes.h"

EngineCore::EngineCore(Settings& settings, Window& window) :m_settings(settings), m_window(window) {
    try {
        // Configure instance
        Instance::Config instanceConfig;
        instanceConfig.enableValidationLayers = true;
        instanceConfig.enableDebugPrintf = false; // Remember that Vulkan configurator must be on for this to work
        instanceConfig.validationLayers = { "VK_LAYER_KHRONOS_validation" };
        instanceConfig.requiredExtensions = VulkanUtils::getRequiredExtensions(instanceConfig.enableValidationLayers);

        // Create Vulkan context
        m_vulkanContext = std::make_unique<VulkanContext>(
            instanceConfig,
            m_window,
            m_settings,
            std::vector<const char*>{VK_KHR_SWAPCHAIN_EXTENSION_NAME}
        );

        // Initialize low-level managers
        m_commandBufferManager = std::make_unique<CommandBufferManager>(*m_vulkanContext);
        m_syncResourceManager = std::make_unique<SynchronizationResourceManager>(m_vulkanContext->logical());
        m_frameManager = std::make_unique<FrameManager>(
            *m_vulkanContext,
            *m_syncResourceManager,
            *m_commandBufferManager,
            m_settings.getFramesInFlight()
        );
        m_vramManager = std::make_unique<VramManager>(
            *m_vulkanContext,
            *m_frameManager,
            *m_commandBufferManager,
            *m_syncResourceManager
        );
        m_shaderModuleManager = std::make_unique<ShaderModuleManager>(
            m_vulkanContext->logical()
        );
        m_descriptorLayoutManager = std::make_unique<DescriptorLayoutManager>(
            m_vulkanContext->logical()
        );
        m_bufferManager = std::make_unique<BufferManager>(
            *m_vramManager
        );
        m_pipelineLayoutManager = std::make_unique<PipelineLayoutManager>(
            m_vulkanContext->logical()
        );
        m_attachmentManager = std::make_unique<AttachmentManager>(
            m_vulkanContext->logical(),
            *m_vramManager
        );
        m_swapChain = std::make_unique<SwapChain>(
            m_vulkanContext->surface(),
            m_vulkanContext->physical(),
            m_vulkanContext->logical(),
            *m_vramManager,
            *m_attachmentManager,
            window,
            settings
        );

        SamplerSettings samplerSettings;
        samplerSettings.textureFiltering = m_settings.getTextureFiltering();
        samplerSettings.mipmapMode = m_settings.getMipmapMode();
        samplerSettings.maxAnisotropy = m_settings.getCurrentAnisotropyValue();
        samplerSettings.anisotropySupported = m_settings.getHardwareLimits().anisotropySupported;

        m_samplerManager = std::make_unique<ImageSamplerManager>(
            m_vulkanContext->logical(),
            samplerSettings
        );

        // Initialize resource managers
        m_renderPassManager = std::make_unique<RenderPassManager>(
            m_vulkanContext->logical()
        );
        m_framebufferManager = std::make_unique<FrameBufferManager>(
            m_vulkanContext->logical(),
            *m_renderPassManager,
            *m_attachmentManager
        );
        m_pipelineManager = std::make_unique<PipelineManager>(
            m_vulkanContext->logical(),
            *m_shaderModuleManager,
            *m_pipelineLayoutManager
        );

        DescriptorAllocator::PoolConfig allocConfig;
        allocConfig.initialSets = 512;
        allocConfig.ratios = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.0f }
        };
        allocConfig.growthFactor = 1.5f;

        m_descriptorAllocator = std::make_unique<DescriptorAllocator>(
            m_vulkanContext->logical(),
            allocConfig
        );

        m_renderGraphManager = std::make_unique<RenderGraphManager>(
            *m_attachmentManager,
			*m_renderPassManager
        );

        // Create renderer
        m_renderer = std::make_unique<Renderer>(
            *this,
            *m_vulkanContext,
            *m_frameManager,
            *m_vramManager,
            *m_swapChain,
            *m_attachmentManager,
            *m_framebufferManager,
            *m_renderPassManager,
            *m_descriptorAllocator,
            *m_pipelineManager,
			*m_commandBufferManager
        );

    }
    catch (const std::exception& e) {
        throw std::runtime_error("Renderer initialization failed: " + std::string(e.what()));
    }
}

EngineCore::~EngineCore() {
    // Wait for all operations to complete before destroying resources
    if (m_vulkanContext && m_vulkanContext->logical().get() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_vulkanContext->logical().get());
    }

    // Reset frame manager to ensure no pending frames hold references
    if (m_frameManager) {
        m_frameManager->waitForAllFrames();
    }

    // Destroy managers in proper order
    m_renderer.reset();
    m_frameManager.reset();
    m_commandBufferManager.reset();

    // Destroy rendering resources
    m_framebufferManager.reset();
    m_pipelineManager.reset();
    m_renderPassManager.reset();
    m_pipelineLayoutManager.reset();
    m_descriptorAllocator.reset();
    m_descriptorLayoutManager.reset();
    m_shaderModuleManager.reset();

    // Destroy render graph system
    m_renderGraphManager.reset();

    // Destroy resource managers
    m_textureManager.reset();
    m_samplerManager.reset();
    m_bufferManager.reset();
    m_swapChain.reset();
    m_attachmentManager.reset();

    // Destroy core managers
    m_vramManager.reset();
    m_syncResourceManager.reset();

    // Finally destroy the Vulkan context
    m_vulkanContext.reset();
}

void EngineCore::advanceFrame()
{
    m_frameManager->advanceFrame();
}


