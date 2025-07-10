#include "Renderer.h"
#include "VulkanUtilities.h"
#include <stdexcept>
#include "Engine.h"
#include "GraphicsTypes.h"

Renderer::Renderer(Settings& settings, Window& window) :m_settings(settings), m_window(window) {
    try {
        // Configure instance
        Instance::Config instanceConfig;
        instanceConfig.enableValidationLayers = true;
        instanceConfig.enableDebugPrintf = false;
        instanceConfig.validationLayers = { "VK_LAYER_KHRONOS_validation" };
        instanceConfig.requiredExtensions = VulkanUtils::getRequiredExtensions(instanceConfig.enableValidationLayers);

        // Create Vulkan context
        m_vulkanContext = std::make_unique<VulkanContext>(
            instanceConfig,
            m_window,
            m_settings,
            std::vector<const char*>{VK_KHR_SWAPCHAIN_EXTENSION_NAME}
        );

        // Initialize managers
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
        m_uniformBufferManager = std::make_unique<UniformBufferManager>(
            *m_vramManager
        );
        m_pipelineLayoutManager = std::make_unique<PipelineLayoutManager>(
            m_vulkanContext->logical()
        );
        m_swapChain = std::make_unique<SwapChain>(
            m_vulkanContext->surface(),
            m_vulkanContext->physical(),
            m_vulkanContext->logical(),
            *m_vramManager,
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
        m_attachmentManager = std::make_unique<AttachmentManager>(
            m_vulkanContext->logical(),
            *m_vramManager
        );
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
		allocConfig.framesInFlight = m_settings.getFramesInFlight();

        m_descriptorAllocator = std::make_unique<DescriptorAllocator>(
            m_vulkanContext->logical(),
            *m_frameManager,
            allocConfig
        );
       
        // Create main render pass
        createMainRenderPass();

        // Create ImGui wrapper after render pass is created
        createImGuiWrapper();

    }
    catch (const std::exception& e) {
        throw std::runtime_error("Renderer initialization failed: " + std::string(e.what()));
    }
}

Renderer::~Renderer() {
    // Wait for all operations to complete before destroying resources
    if (m_vulkanContext && m_vulkanContext->logical().get() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_vulkanContext->logical().get());
    }

    // Reset frame manager to ensure no pending frames hold references
    if (m_frameManager) {
        m_frameManager->waitForAllFrames();
    }

    m_imguiWrapper.reset();

    // Now destroy managers in proper order
    // Destroy high-level managers first
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

    // Destroy resource managers
    m_attachmentManager.reset();
    m_samplerManager.reset();
    m_uniformBufferManager.reset();
    m_swapChain.reset();

    // Destroy core managers
    m_vramManager.reset();
    m_syncResourceManager.reset();

    // Finally destroy the Vulkan context
    m_vulkanContext.reset();
}

void Renderer::advanceFrame()
{
	m_frameManager->advanceFrame();
	m_descriptorAllocator->advanceFrame();
}


void Renderer::recreateSwapChain() {
    SPDLOG_INFO("Starting swapchain recreation");

    // First, wait for all queues to be idle - this is safer than waiting for fences
    vkQueueWaitIdle(m_vulkanContext->logical().getQueue(LogicalDevice::QueueType::Graphics));
    vkQueueWaitIdle(m_vulkanContext->logical().getQueue(LogicalDevice::QueueType::Transfer));

    // Wait for all frames to complete to ensure no descriptor sets are in use
    m_frameManager->waitForAllFrames();

    // Reset all frame command buffers to a clean state
    m_frameManager->resetAllFrames();

    // Now wait for device to be completely idle
    vkDeviceWaitIdle(m_vulkanContext->logical().get());

    SPDLOG_INFO("Device is idle, proceeding with swapchain recreation");

    // Recreate the swap chain
    m_swapChain->recreateSwapChain();

    // Swap chain recreation affects attachments and render pass
    recreateAttachments();
    recreateRenderPass();
    m_imguiWrapper->recreate();

    SPDLOG_INFO("Swapchain recreation completed successfully");
}

void Renderer::recreateAttachments() {
    // Get current settings
    VkSampleCountFlagBits samples = Graphics::convertSampleCount(m_settings.getMsaaSamples());
    VkExtent2D newExtent = m_swapChain->getSwapChainExtent();

    // Release existing attachments
    if (m_attachmentManager->isValid(m_msColorAttachmentHandle)) {
        m_attachmentManager->releaseResource(m_msColorAttachmentHandle);
    }
    if (m_attachmentManager->isValid(m_depthAttachmentHandle)) {
        m_attachmentManager->releaseResource(m_depthAttachmentHandle);
    }

    // Create new MSAA color attachment
    AttachmentSpec msColorAttachmentSpec{
        .format = m_swapChain->getImageFormat(),
        .extent = newExtent,
        .samples = samples,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .type = AttachmentType::Color
    };

    // Create new depth attachment
    VkFormat depthFormat = m_vulkanContext->physical().findDepthFormat();
    AttachmentSpec depthAttachmentSpec{
        .format = depthFormat,
        .extent = newExtent,
        .samples = samples,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .type = AttachmentType::Depth
    };

    // Acquire new attachments
    m_msColorAttachmentHandle = m_attachmentManager->acquireAttachment(msColorAttachmentSpec);
    m_depthAttachmentHandle = m_attachmentManager->acquireAttachment(depthAttachmentSpec);

    // Notify framebuffer manager of the resize
    m_framebufferManager->onResize(newExtent);
}

void Renderer::recreateRenderPass() {
    // Get current settings
    VkSampleCountFlagBits samples = Graphics::convertSampleCount(m_settings.getMsaaSamples());

    // Create new render pass configuration
    RenderPassConfig renderPassConfig;

    // MSAA color attachment description
    RenderPassConfig::AttachmentDesc msColorAttachDesc{
        .format = m_swapChain->getImageFormat(),
        .samples = samples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .type = AttachmentType::Color
    };

    // Resolve attachment description
    RenderPassConfig::AttachmentDesc resolveAttachDesc{
        .format = m_swapChain->getImageFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .type = AttachmentType::Resolve
    };

    // Depth attachment description
    VkFormat depthFormat = m_vulkanContext->physical().findDepthFormat();
    RenderPassConfig::AttachmentDesc depthAttachDesc{
        .format = depthFormat,
        .samples = samples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .type = AttachmentType::Depth
    };

    // Configure render pass
    renderPassConfig.attachments = { msColorAttachDesc, resolveAttachDesc, depthAttachDesc };
    renderPassConfig.colorAttachmentIndices = { 0 };
    renderPassConfig.resolveAttachmentIndex = 1;
    renderPassConfig.depthAttachmentIndex = 2;

    // Recreate render pass with new configuration
    m_renderPassManager->recreateRenderPass(m_mainRenderPassHandle, renderPassConfig);
}

void Renderer::createMainRenderPass() {
    VkSampleCountFlagBits samples = Graphics::convertSampleCount(m_settings.getMsaaSamples());

    // Create a multisampled color attachment (not for presentation)
    AttachmentSpec msColorAttachmentSpec{
        .format = m_swapChain->getImageFormat(),
        .extent = m_swapChain->getSwapChainExtent(),
        .samples = samples, // Multisampled (4x MSAA in your case)
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .type = AttachmentType::Color
    };

    // Create a resolve attachment (for presentation, single sample)
    AttachmentSpec resolveAttachmentSpec{
        .format = m_swapChain->getImageFormat(),
        .extent = m_swapChain->getSwapChainExtent(),
        .samples = VK_SAMPLE_COUNT_1_BIT, // Single sample for presentation
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .type = AttachmentType::Resolve
    };

    // Create/retrieve a depth attachment
    VkFormat depthFormat = m_vulkanContext->physical().findDepthFormat();
    AttachmentSpec depthAttachmentSpec{
        .format = depthFormat,
        .extent = m_swapChain->getSwapChainExtent(),
        .samples = samples, // Same as multisampled color attachment
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .type = AttachmentType::Depth
    };

    // Create all attachments
    m_msColorAttachmentHandle = m_attachmentManager->acquireAttachment(msColorAttachmentSpec);
    // Note: Resolve attachment will typically come from the swapchain
    m_depthAttachmentHandle = m_attachmentManager->acquireAttachment(depthAttachmentSpec);

    // Configure render pass
    RenderPassConfig renderPassConfig;

    // Multisampled color attachment description
    RenderPassConfig::AttachmentDesc msColorAttachDesc{
        .format = msColorAttachmentSpec.format,
        .samples = msColorAttachmentSpec.samples, // Multisampled
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // We don't need to store this, just resolve it
        .initialLayout = msColorAttachmentSpec.initialLayout,
        .finalLayout = msColorAttachmentSpec.finalLayout,
        .type = AttachmentType::Color
    };

    // Resolve attachment description (for the swapchain image)
    RenderPassConfig::AttachmentDesc resolveAttachDesc{
        .format = resolveAttachmentSpec.format,
        .samples = resolveAttachmentSpec.samples, // Single sample
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, // We don't need to load this since we're resolving to it
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE, // Store for presentation
        .initialLayout = resolveAttachmentSpec.initialLayout,
        .finalLayout = resolveAttachmentSpec.finalLayout,
        .type = AttachmentType::Resolve
    };

    // Depth attachment description
    RenderPassConfig::AttachmentDesc depthAttachDesc{
        .format = depthAttachmentSpec.format,
        .samples = depthAttachmentSpec.samples, // Multisampled
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = depthAttachmentSpec.initialLayout,
        .finalLayout = depthAttachmentSpec.finalLayout,
        .type = AttachmentType::Depth
    };

    // Add attachments to config
    renderPassConfig.attachments.push_back(msColorAttachDesc);    // Index 0: Multisampled color
    renderPassConfig.attachments.push_back(resolveAttachDesc);    // Index 1: Resolve target
    renderPassConfig.attachments.push_back(depthAttachDesc);      // Index 2: Depth

    // Set indices
    renderPassConfig.colorAttachmentIndices.push_back(0);         // Multisampled color is the main color attachment
    renderPassConfig.resolveAttachmentIndex = 1;                  // Resolve from index 0 to index 1
    renderPassConfig.depthAttachmentIndex = 2;                    // Depth attachment index

    // Create/retrieve the render pass
    m_mainRenderPassHandle = m_renderPassManager->acquireRenderPass(renderPassConfig);
}

void Renderer::createImGuiWrapper() {
    uint32_t imageCount = m_settings.getFramesInFlight();
    uint32_t minImageCount = imageCount;
    VkSampleCountFlagBits samples = Graphics::convertSampleCount(m_settings.getMsaaSamples());

    m_imguiWrapper = std::make_unique<ImGuiWrapper>(
        m_window,
        *m_vulkanContext,
        m_mainRenderPassHandle,
        *m_renderPassManager,
        minImageCount,
        imageCount,
        samples
    );
}