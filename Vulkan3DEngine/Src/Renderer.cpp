#include "Renderer.h"
#include "VulkanUtilities.h"
#include <stdexcept>
#include "Engine.h"
#include "GraphicsTypes.h"

Renderer::Renderer(Window& window) : m_window(window) {
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
            std::vector<const char*>{VK_KHR_SWAPCHAIN_EXTENSION_NAME}
        );

        // Initialize managers
        m_commandBufferManager = std::make_unique<CommandBufferManager>(*m_vulkanContext);
        m_syncResourceManager = std::make_unique<SynchronizationResourceManager>(m_vulkanContext->logical());
        m_frameManager = std::make_unique<FrameManager>(
            *m_vulkanContext,
            *m_syncResourceManager,
            *m_commandBufferManager,
            Engine::get().settings().getFramesInFlight()
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
            *m_vramManager
        );
        m_samplerManager = std::make_unique<ImageSamplerManager>(
            m_vulkanContext->logical()
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

        m_descriptorAllocator = std::make_unique<DescriptorAllocator>(
            m_vulkanContext->logical(),
            allocConfig
        );
       
        // Create main render pass
        createMainRenderPass();

    }
    catch (const std::exception& e) {
        throw std::runtime_error("Renderer initialization failed: " + std::string(e.what()));
    }
}

Renderer::~Renderer() {
    // Wait for all operations to complete before destroying resources
    vkDeviceWaitIdle(m_vulkanContext->logical().get());

    // Automatic resource cleanup by unique_ptr in reverse order of initialization
}

void Renderer::createMainRenderPass() {
    Settings& settings = Engine::get().settings();
    VkSampleCountFlagBits samples = Graphics::convertSampleCount(settings.getMsaaSamples());

    // Create a multisampled color attachment (not for presentation)
    AttachmentSpec msColorAttachmentSpec{
        .format = m_swapChain->getImageFormat(),
        .extent = m_swapChain->getSwapChainExtent(),
        .samples = samples, // Multisampled (4x MSAA in your case)
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .type = AttachmentType::Color,
        .name = "MSAAColorAttachment"
    };

    // Create a resolve attachment (for presentation, single sample)
    AttachmentSpec resolveAttachmentSpec{
        .format = m_swapChain->getImageFormat(),
        .extent = m_swapChain->getSwapChainExtent(),
        .samples = VK_SAMPLE_COUNT_1_BIT, // Single sample for presentation
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .type = AttachmentType::Resolve,
        .name = "ResolveAttachment"
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
        .type = AttachmentType::Depth,
        .name = "MainDepthAttachment"
    };

    // Create all attachments
    m_msColorAttachmentHandle = m_attachmentManager->getOrCreate(msColorAttachmentSpec);
    // Note: Resolve attachment will typically come from the swapchain
    m_depthAttachmentHandle = m_attachmentManager->getOrCreate(depthAttachmentSpec);

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
    m_mainRenderPassHandle = m_renderPassManager->getOrCreate(renderPassConfig);
}

// Also update the recreateRenderResources method to handle the multisampled color attachment

void Renderer::recreateRenderResources() {
    vkDeviceWaitIdle(m_vulkanContext->logical().get());

    // Update MSAA sample count if needed
    Settings& settings = Engine::get().settings();
    VkSampleCountFlagBits samples = Graphics::convertSampleCount(settings.getMsaaSamples());

    // Create a new multisampled color attachment
    AttachmentSpec msColorAttachmentSpec{
        .format = m_swapChain->getImageFormat(),
        .extent = m_swapChain->getSwapChainExtent(),
        .samples = samples,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .type = AttachmentType::Color,
        .name = "MSAAColorAttachment"
    };

    // Recreate depth attachment with new swapchain extent
    VkFormat depthFormat = m_vulkanContext->physical().findDepthFormat();
    AttachmentSpec depthAttachmentSpec{
        .format = depthFormat,
        .extent = m_swapChain->getSwapChainExtent(),
        .samples = samples,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .type = AttachmentType::Depth,
        .name = "MainDepthAttachment"
    };

    // Destroy old attachments if they exist
    if (m_attachmentManager->isValid(m_msColorAttachmentHandle)) {
        m_attachmentManager->destroy(m_msColorAttachmentHandle);
    }
    if (m_attachmentManager->isValid(m_depthAttachmentHandle)) {
        m_attachmentManager->destroy(m_depthAttachmentHandle);
    }

    // Create new attachments 
    m_msColorAttachmentHandle = m_attachmentManager->getOrCreate(msColorAttachmentSpec);
    m_depthAttachmentHandle = m_attachmentManager->getOrCreate(depthAttachmentSpec);

    // Notify the framebuffer manager of resize
    m_framebufferManager->onResize(m_swapChain->getSwapChainExtent());

    // Main framebuffers will be recreated on-demand in drawFrame
}