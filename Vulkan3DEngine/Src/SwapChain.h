#pragma once
#include "Prerequisites.h"
#include <vector>

class SwapChain
{
public:
	SwapChain(Renderer* renderer);
	~SwapChain();

	VkRenderPass getRenderPass() { return m_renderPass; };
	std::vector<VkFramebuffer> getSwapChainFramebuffers() { return m_swapChainFramebuffers; };
	VkExtent2D getSwapChainExtent() { return m_swapChainExtent; };
	void recreateSwapChain();
private:
	void createSwapChain();
	void cleanupSwapChain();
	void createImageViews();
	void createRenderPass();
	void createColorResources();
	void createDepthResources();
	void createFramebuffers();
	void createSyncObjects();

	Renderer* m_renderer = nullptr;
	VkSwapchainKHR m_swapChain;

	std::vector<VkImage> m_swapChainImages;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	std::vector<VkImageView> m_swapChainImageViews;

	VkRenderPass m_renderPass;

	ImagePtr m_depthImage;
	VkImageView m_depthImageView;

	ImagePtr m_colorImage;
	VkImageView m_colorImageView;

	std::vector<VkFramebuffer> m_swapChainFramebuffers;

	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;

	friend class Renderer;
};

