#include "SwapChain.h"

SwapChain::SwapChain(Device* device): m_device(device)
{
	createSwapChain();
	createImageViews();
}

SwapChain::~SwapChain()
{
	for (auto imageView : m_swap_chain_image_views) {
		vkDestroyImageView(*m_device->getDevice(), imageView, nullptr);
		
	}
	vkDestroySwapchainKHR(*m_device->getDevice(), m_swap_chain, nullptr);
}

void SwapChain::createSwapChain()
{
	SwapChainSupportDetails swapChainSupport = m_device->querySwapChainSupport(*m_device->getPhysicalDevice());
	VkSurfaceFormatKHR surfaceFormat = m_device->chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = m_device->chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = m_device->chooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 &&
		imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = *m_device->getSurface();

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = m_device->findQueueFamilies(*m_device->getPhysicalDevice());
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

	if (vkCreateSwapchainKHR(*m_device->getDevice(), &createInfo, nullptr, &m_swap_chain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(*m_device->getDevice(), m_swap_chain, &imageCount, nullptr);
	m_swap_chain_images.resize(imageCount);
	vkGetSwapchainImagesKHR(*m_device->getDevice(), m_swap_chain, &imageCount, m_swap_chain_images.data());

	m_swap_chain_image_format = surfaceFormat.format;
	m_swap_chain_extent = extent;
}

void SwapChain::createImageViews()
{
	m_swap_chain_image_views.resize(m_swap_chain_images.size());

	for (size_t i = 0; i < m_swap_chain_images.size(); i++) {
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_swap_chain_images[i];

		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_swap_chain_image_format;

		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(*m_device->getDevice(), &createInfo, nullptr,
			&m_swap_chain_image_views[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image views!");
		}

	}
}
