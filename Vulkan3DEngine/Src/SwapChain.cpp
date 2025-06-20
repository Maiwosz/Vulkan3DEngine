#include "SwapChain.h"
#include "VulkanUtilities.h"
#include "Engine.h"

SwapChain::SwapChain(
    const Surface& surface,
    const PhysicalDevice& physicalDevice,
    const LogicalDevice& logicalDevice,
    VramManager& vramManager,
    Window& window,
    Settings& settings
) :
    m_surface(surface),
    m_physicalDevice(physicalDevice),
    m_logicalDevice(logicalDevice),
    m_vramManager(vramManager),
    m_window(window),
    m_settings(settings)
{
    try {
        init();
        fmt::print("SwapChain created successfully\n");
    }
    catch (const std::exception& e) {
        fmt::print("Error during SwapChain creation: {}\n", e.what());
        throw;
    }
}

SwapChain::~SwapChain()
{
    cleanupSwapChain();
    fmt::print("SwapChain destroyed successfully\n");
}

void SwapChain::init() {
    createSwapChain();
    createImageViews();
    registerImagesWithVramManager();
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

    // Make sure image count is compatible with hardware capabilities
    if (imageCount < swapChainSupport.capabilities.minImageCount) {
        imageCount = swapChainSupport.capabilities.minImageCount;
    }

    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
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
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_logicalDevice.get(), &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        fmt::print("Failed to create swap chain!\n");
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(m_logicalDevice.get(), m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_logicalDevice.get(), m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;

    fmt::print("Swap chain created with {} images\n", imageCount);
    fmt::print("VSync: {}\n", m_settings.isVsyncEnabled() ? "enabled" : "disabled");
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
    cleanupSwapChain();

    try {
        init();
        fmt::print("Swap chain recreated successfully\n");
    }
    catch (const std::exception& e) {
        fmt::print("Error during SwapChain recreation: {}\n", e.what());
        throw;
    }
}

void SwapChain::cleanupSwapChain()
{
    for (auto handle : m_imageHandles) {
        if (handle.isValid()) {
            m_vramManager.freeResource(handle);
        }
    }
    m_imageHandles.clear();

    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(m_logicalDevice.get(), imageView, nullptr);
    }

    vkDestroySwapchainKHR(m_logicalDevice.get(), m_swapChain, nullptr);

    fmt::print("Swap chain cleaned up\n");
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
            fmt::print("Failed to create swap chain image view for image {}\n", i);
            throw std::runtime_error(fmt::format("Failed to create swap chain image view {}", i));
        }
    }

    fmt::print("Created {} swap chain image views\n", m_swapChainImages.size());
}

void SwapChain::registerImagesWithVramManager() {
    m_imageHandles.resize(m_swapChainImages.size());

    for (size_t i = 0; i < m_swapChainImages.size(); i++) {
        m_imageHandles[i] = m_vramManager.registerExternalImage(
            m_swapChainImages[i],          // VkImage
            m_swapChainImageFormat,         // VkFormat
            m_swapChainExtent,              // VkExtent2D
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // initialLayout
            VK_SAMPLE_COUNT_1_BIT           // samples
        );
    }
}
