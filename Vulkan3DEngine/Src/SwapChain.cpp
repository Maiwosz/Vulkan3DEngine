#include "SwapChain.h"
#include "Renderer.h"
#include "Image.h"
#include "GraphicsEngine.h"
#include "RendererInits.h"

SwapChain::SwapChain(Renderer* renderer) : m_renderer(renderer)
{
    try {
        createSwapChain();
        createImageViews();
        createRenderPass();
        createColorResources();
        createDepthResources();
        createFramebuffers();
        createSyncObjects();
        fmt::print("SwapChain created successfully\n");
    }
    catch (const std::exception& e) {
        fmt::print("Error during SwapChain creation: {}\n", e.what());
        throw;
    }
}

SwapChain::~SwapChain()
{
    for (size_t i = 0; i < Renderer::s_maxFramesInFlight; i++) {
        vkDestroySemaphore(GraphicsEngine::get()->getDevice()->get(), m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(GraphicsEngine::get()->getDevice()->get(), m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(GraphicsEngine::get()->getDevice()->get(), m_inFlightFences[i], nullptr);
    }

    cleanupSwapChain();
    //vkDestroyRenderPass(GraphicsEngine::get()->getDevice()->get(), m_renderPass, nullptr);

    fmt::print("SwapChain destroyed successfully\n");
}

void SwapChain::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = GraphicsEngine::get()->getDevice()->querySwapChainSupport(GraphicsEngine::get()->getDevice()->getPhysicalDevice());
    VkSurfaceFormatKHR surfaceFormat = GraphicsEngine::get()->getDevice()->chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = GraphicsEngine::get()->getDevice()->chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = GraphicsEngine::get()->getDevice()->chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = Window::get()->getSurface();

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = GraphicsEngine::get()->getDevice()->findQueueFamilies(GraphicsEngine::get()->getDevice()->getPhysicalDevice());
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(GraphicsEngine::get()->getDevice()->get(), &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        fmt::print("Failed to create swap chain!\n");
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(GraphicsEngine::get()->getDevice()->get(), m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(GraphicsEngine::get()->getDevice()->get(), m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;

    fmt::print("Swap chain created with {} images\n", imageCount);
}

void SwapChain::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(Window::get()->getWindow(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(Window::get()->getWindow(), &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());

    cleanupSwapChain();

    try {
        createSwapChain();
        createImageViews();
        createRenderPass();
        createColorResources();
        createDepthResources();
        createFramebuffers();
        fmt::print("Swap chain recreated successfully\n");
    }
    catch (const std::exception& e) {
        fmt::print("Error during SwapChain recreation: {}\n", e.what());
        throw;
    }
}

void SwapChain::cleanupSwapChain()
{
    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(GraphicsEngine::get()->getDevice()->get(), framebuffer, nullptr);
    }

    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(GraphicsEngine::get()->getDevice()->get(), imageView, nullptr);
    }
    if (m_colorImageView) {
        vkDestroyImageView(GraphicsEngine::get()->getDevice()->get(), m_colorImageView, nullptr);
    }
    vkDestroyImageView(GraphicsEngine::get()->getDevice()->get(), m_depthImageView, nullptr);

    vkDestroySwapchainKHR(GraphicsEngine::get()->getDevice()->get(), m_swapChain, nullptr);

    vkDestroyRenderPass(GraphicsEngine::get()->getDevice()->get(), m_renderPass, nullptr);

    fmt::print("Swap chain cleaned up\n");
}

void SwapChain::createImageViews()
{
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (uint32_t i = 0; i < m_swapChainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo = RendererInits::imageviewCreateInfo(m_swapChainImages[i], m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        if (vkCreateImageView(GraphicsEngine::get()->getDevice()->get(), &viewInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS) {
            fmt::print("Failed to create swap chain image view for image {}\n", i);
            throw std::runtime_error(fmt::format("failed to create swap chain image view for image {}", i));
        }
    }

    fmt::print("Created image views for {} swap chain images\n", m_swapChainImages.size());
}

void SwapChain::createRenderPass() {
    VkSampleCountFlagBits msaaSamples = Renderer::s_msaaSamples;

    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = (msaaSamples == VK_SAMPLE_COUNT_1_BIT) ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = GraphicsEngine::get()->getDevice()->findDepthFormat();
    depthAttachment.samples = msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Resolve attachment
    VkAttachmentDescription colorAttachmentResolve{};
    if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
        colorAttachmentResolve.format = m_swapChainImageFormat;
        colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    // Attachment references
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef{};
    if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
        colorAttachmentResolveRef.attachment = 2;
        colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // Subpass description
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
        subpass.pResolveAttachments = &colorAttachmentResolveRef;
    }

    // Subpass dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Attachments array
    std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };
    if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
        attachments.push_back(colorAttachmentResolve);
    }

    // Render pass creation
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(GraphicsEngine::get()->getDevice()->get(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void SwapChain::createColorResources()
{
    if (Renderer::s_msaaSamples == VK_SAMPLE_COUNT_1_BIT) {
        return;
    }

    VkFormat colorFormat = m_swapChainImageFormat;

    m_colorImage = m_renderer->createImage(m_swapChainExtent.width, m_swapChainExtent.height, 1, Renderer::s_msaaSamples, colorFormat,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageViewCreateInfo viewInfo = RendererInits::imageviewCreateInfo(m_colorImage->get(), colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    if (vkCreateImageView(GraphicsEngine::get()->getDevice()->get(), &viewInfo, nullptr, &m_colorImageView) != VK_SUCCESS) {
        fmt::print("Failed to create swapChain colorImageView\n");
        throw std::runtime_error("failed to create swapChain colorImageView!");
    }

    fmt::print("Color resources created\n");
}


void SwapChain::createDepthResources()
{
    VkFormat depthFormat = GraphicsEngine::get()->getDevice()->findDepthFormat();

    m_depthImage = m_renderer->createImage(m_swapChainExtent.width, m_swapChainExtent.height, 1, Renderer::s_msaaSamples, depthFormat,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageViewCreateInfo viewInfo = RendererInits::imageviewCreateInfo(m_depthImage->get(), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
    if (vkCreateImageView(GraphicsEngine::get()->getDevice()->get(), &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS) {
        fmt::print("Failed to create swapChain depthImageView\n");
        throw std::runtime_error("failed to create swapChain depthImageView!");
    }

    m_depthImage->transitionImageLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    fmt::print("Depth resources created\n");
}

void SwapChain::createFramebuffers() {
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

    for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
        std::vector<VkImageView> attachments;
        if (Renderer::s_msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
            attachments = { m_colorImageView, m_depthImageView, m_swapChainImageViews[i] };
        }
        else {
            attachments = { m_swapChainImageViews[i], m_depthImageView };
        }

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_swapChainExtent.width;
        framebufferInfo.height = m_swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(GraphicsEngine::get()->getDevice()->get(), &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void SwapChain::createSyncObjects()
{
    int maxFramesInFlight = Renderer::s_maxFramesInFlight;

    m_imageAvailableSemaphores.resize(maxFramesInFlight);
    m_renderFinishedSemaphores.resize(maxFramesInFlight);
    m_inFlightFences.resize(maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        if (vkCreateSemaphore(GraphicsEngine::get()->getDevice()->get(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(GraphicsEngine::get()->getDevice()->get(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(GraphicsEngine::get()->getDevice()->get(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            fmt::print("Failed to create synchronization objects for frame {}\n", i);
            throw std::runtime_error(fmt::format("failed to create synchronization objects for frame {}", i));
        }
    }

    fmt::print("Synchronization objects created\n");
}