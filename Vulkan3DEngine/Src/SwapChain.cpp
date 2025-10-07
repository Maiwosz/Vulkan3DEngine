#include "SwapChain.h"
#include "VulkanUtilities.h"
#include "Engine.h"
#include <spdlog/spdlog.h>

SwapChain::SwapChain(
    const Surface& surface,
    const PhysicalDevice& physicalDevice,
    const LogicalDevice& logicalDevice,
    VramManager& vramManager,
    AttachmentManager& attachmentManager,
    Window& window,
    Settings& settings
) :
    m_surface(surface),
    m_physicalDevice(physicalDevice),
    m_logicalDevice(logicalDevice),
    m_vramManager(vramManager),
    m_attachmentManager(attachmentManager),
    m_window(window),
    m_settings(settings),
    m_framesInFlight(0)
{
    try {
        init();
        SPDLOG_INFO("SwapChain created successfully");
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error during SwapChain creation: {}", e.what());
        throw;
    }
}

SwapChain::~SwapChain()
{
    cleanupSwapChain();
    SPDLOG_INFO("SwapChain destroyed successfully");
}

void SwapChain::init() {
    createSwapChain();
    createImageViews();
    registerImagesWithVramManager();
    registerImagesWithAttachmentManager(false);
}

void SwapChain::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = PhysicalDevice::querySwapChainSupport(m_physicalDevice.get(), m_surface.get());
    VkSurfaceFormatKHR surfaceFormat = VulkanUtils::chooseSwapSurfaceFormat(swapChainSupport.formats);

    // Use presentation mode based on VSync setting
    VkPresentModeKHR presentMode = getVulkanPresentMode(swapChainSupport.presentModes);

    // SwapChain dimensions calculated as before
    VkExtent2D extent = VulkanUtils::chooseSwapExtent(swapChainSupport.capabilities, m_window.get());

    // Use frames in flight from settings
    uint32_t imageCount = m_settings.getFramesInFlight();
    m_framesInFlight = imageCount;

    // Make sure image count is compatible with hardware capabilities
    if (imageCount < swapChainSupport.capabilities.minImageCount) {
        imageCount = swapChainSupport.capabilities.minImageCount;
        m_framesInFlight = imageCount;
    }

    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
        m_framesInFlight = imageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface.get();

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = m_physicalDevice.findQueueFamilies();
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_logicalDevice.get(), &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        SPDLOG_ERROR("Failed to create swap chain!");
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(m_logicalDevice.get(), m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_logicalDevice.get(), m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;

    SPDLOG_INFO("Swap chain created with {} images", imageCount);
    SPDLOG_INFO("VSync: {}", m_settings.isVsyncEnabled() ? "enabled" : "disabled");
}

VkPresentModeKHR SwapChain::getVulkanPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const {
    // If VSync is enabled, use FIFO (guaranteed)
    if (m_settings.isVsyncEnabled()) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // Otherwise, prefer mailbox mode (if available)
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    // If mailbox is not available, try immediate
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    // Finally FIFO as a safe solution (always available)
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkResult SwapChain::acquireNextImage(
    VkSemaphore imageAvailableSemaphore,
    uint32_t* pImageIndex,
    uint64_t timeout)
{
    VkResult result = vkAcquireNextImageKHR(
        m_logicalDevice.get(),
        m_swapChain,
        timeout,
        imageAvailableSemaphore,
        VK_NULL_HANDLE,
        pImageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
    }

    return result;
}

VkResult SwapChain::presentImage(uint32_t imageIndex, VkSemaphore renderFinishedSemaphore) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapChain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    VkResult result = vkQueuePresentKHR(m_logicalDevice.getQueue(LogicalDevice::QueueType::Present), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    }

    return result;
}

void SwapChain::recreateSwapChain()
{
    vkDeviceWaitIdle(m_logicalDevice.get());

    // Store old attachment handles
    auto oldAttachmentHandles = m_attachmentHandles;

    cleanupSwapChain();

    try {
        createSwapChain();
        createImageViews();
        registerImagesWithVramManager();

        // Restore old handles and recreate attachments
        m_attachmentHandles = oldAttachmentHandles;
        registerImagesWithAttachmentManager(true);

        SPDLOG_INFO("Swap chain recreated successfully");
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error during SwapChain recreation: {}", e.what());
        throw;
    }
}

void SwapChain::cleanupSwapChain()
{
    // NIE uwalniamy attachment handles - będą ponownie użyte

    // Free VRAM handles
    for (auto handle : m_imageHandles) {
        if (handle.isValid()) {
            m_vramManager.freeResource(handle);
        }
    }
    m_imageHandles.clear();

    // Destroy image views
    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(m_logicalDevice.get(), imageView, nullptr);
    }
    m_swapChainImageViews.clear();

    // Destroy swapchain
    vkDestroySwapchainKHR(m_logicalDevice.get(), m_swapChain, nullptr);

    SPDLOG_DEBUG("Swap chain cleaned up");
}

void SwapChain::createImageViews() {
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (uint32_t i = 0; i < m_swapChainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapChainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_swapChainImageFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_logicalDevice.get(), &viewInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS) {
            SPDLOG_ERROR("Failed to create swap chain image view for image {}", i);
            throw std::runtime_error(fmt::format("Failed to create swap chain image view {}", i));
        }
    }

    SPDLOG_DEBUG("Created {} swap chain image views", m_swapChainImages.size());
}

void SwapChain::registerImagesWithVramManager() {
    m_imageHandles.resize(m_swapChainImages.size());

    for (size_t i = 0; i < m_swapChainImages.size(); i++) {
        m_imageHandles[i] = m_vramManager.registerExternalImage(
            m_swapChainImages[i],
            m_swapChainImageFormat,
            m_swapChainExtent,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_SAMPLE_COUNT_1_BIT
        );
    }

    SPDLOG_DEBUG("Registered {} images with VRAM manager", m_swapChainImages.size());
}

void SwapChain::registerImagesWithAttachmentManager(bool isRecreate) {
    if (!isRecreate) {
        m_attachmentHandles.resize(m_imageHandles.size());
    }

    for (size_t i = 0; i < m_imageHandles.size(); i++) {
        if (isRecreate && i < m_attachmentHandles.size()) {
            // Recreate existing attachment with new image
            AttachmentImageSpec spec;
            spec.format = m_swapChainImageFormat;
            spec.extent = m_swapChainExtent;
            spec.samples = VK_SAMPLE_COUNT_1_BIT;
            spec.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            spec.type = AttachmentType::Color;

            m_attachmentManager.recreateAttachment(m_attachmentHandles[i], spec);
        }
        else {
            // Register new external image
            m_attachmentHandles[i] = m_attachmentManager.registerExternalImage(
                m_imageHandles[i],
                m_swapChainImageFormat,
                m_swapChainExtent,
                AttachmentType::Color,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            );
        }
    }

    SPDLOG_DEBUG("Registered {} attachments with AttachmentManager", m_attachmentHandles.size());
}

void SwapChain::releaseAttachments() {
    for (auto handle : m_attachmentHandles) {
        if (m_attachmentManager.isValid(handle)) {
            m_attachmentManager.releaseResource(handle);
        }
    }
    m_attachmentHandles.clear();
}