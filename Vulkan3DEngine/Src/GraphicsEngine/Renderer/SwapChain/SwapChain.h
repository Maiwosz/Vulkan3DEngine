#pragma once
#include "..\..\..\Prerequisites.h"
#include "..\..\GraphicsEngine.h"
#include "..\..\Device\Device.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

class SwapChain
{
public:
	SwapChain(Device& device);
	~SwapChain();
private:
	void createSwapChain();
	void createImageViews();

	Device& m_device;
	VkSwapchainKHR m_swap_chain;

	std::vector<VkImage> m_swap_chain_images;
	VkFormat m_swap_chain_image_format;
	VkExtent2D m_swap_chain_extent;

	std::vector<VkImageView> m_swap_chain_image_views;

	friend class GraphicsPipeline;
};

