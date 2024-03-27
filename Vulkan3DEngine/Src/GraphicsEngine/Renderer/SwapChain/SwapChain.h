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
	SwapChain(DevicePtr device, Renderer* renderer);
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

	DevicePtr m_device;
	Renderer* m_renderer;
	VkSwapchainKHR m_swapChain;

	std::vector<VkImage> m_swapChainImages;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	std::vector<VkImageView> m_swapChainImageViews;

	VkRenderPass m_renderPass;

	std::vector<VkFramebuffer> m_swapChainFramebuffers;

	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;


	friend class GraphicsPipeline;
	friend class Renderer;
};

