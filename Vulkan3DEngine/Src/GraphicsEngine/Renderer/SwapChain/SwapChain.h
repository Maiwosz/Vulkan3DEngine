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

	VkRenderPass getRenderPass() { return m_renderPass; };
	std::vector<VkFramebuffer> getSwapChainFramebuffers() { return m_swapChainFramebuffers; };
	VkExtent2D getSwapChainExtent() { return m_swapChainExtent; };
private:
	void createSwapChain();
	void createImageViews();
	void createRenderPass();
	void createFramebuffers();
	void createSyncObjects();

	Device& m_device;
	VkSwapchainKHR m_swapChain;

	std::vector<VkImage> m_swapChainImages;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	std::vector<VkImageView> m_swapChainImageViews;

	VkRenderPass m_renderPass;

	std::vector<VkFramebuffer> m_swapChainFramebuffers;

	VkSemaphore m_imageAvailableSemaphore;
	VkSemaphore m_renderFinishedSemaphore;
	VkFence m_inFlightFence;

	friend class GraphicsPipeline;
	friend class Renderer;
};

