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
        m_shaderModuleManager = std::make_unique<ShaderModuleManager>(
            m_vulkanContext->logical(),
            *m_uniformBufferManager,
            *m_descriptorLayoutManager,
            *m_pipelineLayoutManager 
        );
        m_pipelineManager = std::make_unique<PipelineManager>(
            m_vulkanContext->logical(),
            *m_shaderModuleManager,
            *m_pipelineLayoutManager 
        );
        m_materialManager = std::make_unique<MaterialManager>(
            *m_shaderModuleManager,
            *m_samplerManager
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
        m_meshManager = std::make_unique<MeshManager>(
            *m_vramManager
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
    // Create/retrieve a color attachment for the swapchain image
    AttachmentSpec colorAttachmentSpec{
        .format = m_swapChain->getImageFormat(),
        .extent = m_swapChain->getSwapChainExtent(),
        .samples = VK_SAMPLE_COUNT_1_BIT, // Can be updated based on MSAA settings
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .type = AttachmentType::Color,
        .name = "MainColorAttachment"
    };

    // Create/retrieve a depth attachment
    VkFormat depthFormat = m_vulkanContext->physical().findDepthFormat();
    AttachmentSpec depthAttachmentSpec{
        .format = depthFormat,
        .extent = m_swapChain->getSwapChainExtent(),
        .samples = VK_SAMPLE_COUNT_1_BIT, // Same as color attachment
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .type = AttachmentType::Depth,
        .name = "MainDepthAttachment"
    };

    // Create the depth attachment (color attachments come from swapchain)
    m_depthAttachmentHandle = m_attachmentManager->getOrCreate(depthAttachmentSpec);

    // Configure render pass
    RenderPassConfig renderPassConfig;

    // Color attachment description
    RenderPassConfig::AttachmentDesc colorAttachDesc{
        .format = colorAttachmentSpec.format,
        .samples = colorAttachmentSpec.samples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // Clear at the beginning of the render pass
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE, // Store for presentation
        .initialLayout = colorAttachmentSpec.initialLayout,
        .finalLayout = colorAttachmentSpec.finalLayout,
        .type = AttachmentType::Color
    };

    // Depth attachment description
    RenderPassConfig::AttachmentDesc depthAttachDesc{
        .format = depthAttachmentSpec.format,
        .samples = depthAttachmentSpec.samples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // Clear at the beginning
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // Don't care about storing depth
        .initialLayout = depthAttachmentSpec.initialLayout,
        .finalLayout = depthAttachmentSpec.finalLayout,
        .type = AttachmentType::Depth
    };

    // Add attachments to config
    renderPassConfig.attachments.push_back(colorAttachDesc);
    renderPassConfig.attachments.push_back(depthAttachDesc);

    // Set indices
    renderPassConfig.colorAttachmentIndices.push_back(0); // First attachment is color
    renderPassConfig.depthAttachmentIndex = 1; // Second attachment is depth

    // Create/retrieve the render pass
    m_mainRenderPassHandle = m_renderPassManager->getOrCreate(renderPassConfig);
}

void Renderer::recreateRenderResources() {
    vkDeviceWaitIdle(m_vulkanContext->logical().get());

    // Update MSAA sample count if needed
    VkSampleCountFlagBits msaaSamples = static_cast<VkSampleCountFlagBits>(
        Engine::get().settings().getCurrentMsaaSampleCount()
        );

    // Recreate depth attachment with new swapchain extent
    VkFormat depthFormat = m_vulkanContext->physical().findDepthFormat();
    AttachmentSpec depthAttachmentSpec{
        .format = depthFormat,
        .extent = m_swapChain->getSwapChainExtent(),
        .samples = msaaSamples,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .type = AttachmentType::Depth,
        .name = "MainDepthAttachment"
    };

    // Destroy old depth attachment if it exists
    if (m_attachmentManager->isValid(m_depthAttachmentHandle)) {
        m_attachmentManager->destroy(m_depthAttachmentHandle);
    }

    // Create new depth attachment
    m_depthAttachmentHandle = m_attachmentManager->getOrCreate(depthAttachmentSpec);

    // Notify the framebuffer manager of resize
    m_framebufferManager->onResize(m_swapChain->getSwapChainExtent());

    // Main framebuffers will be recreated on-demand in drawFrame
}

void Renderer::handleWindowResize(int width, int height) {
    // Ignore resize events with zero size (e.g., during minimization)
    if (width == 0 || height == 0) {
        return;
    }

    // Wait for all operations to complete
    vkDeviceWaitIdle(m_vulkanContext->logical().get());

    // The SwapChain will be recreated through its own internal handling
    m_swapChain->recreateSwapChain();
}