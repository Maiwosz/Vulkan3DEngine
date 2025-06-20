#pragma once
#include "Prerequisites.h"
#include <vector>
#include "LogicalDevice.h"
#include "Surface.h"
#include "VramManager.h"
#include "Event.h"

class SwapChain
{
public:
	SwapChain(
		const Surface& surface,
		const PhysicalDevice& physicalDevice,
		const LogicalDevice& logicalDevice,
		VramManager& vramManager,
		Window& window,
		Settings& settings
	);
	~SwapChain();

	VkSwapchainKHR get() const { return m_swapChain; }
	VkExtent2D getSwapChainExtent() const { return m_swapChainExtent; };
	VkFormat getImageFormat() const { return m_swapChainImageFormat; }
	const std::vector<VkImageView>& getImageViews() const { return m_swapChainImageViews; }
	const std::vector<VkImage>& getImages() const { return m_swapChainImages; }
	const std::vector<VramHandle>& getImageHandles() const { return m_imageHandles; }

	VkResult acquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t* pImageIndex, uint64_t timeout = UINT64_MAX);
	VkResult presentImage(uint32_t imageIndex, VkSemaphore renderFinishedSemaphore);
	void recreateSwapChain();

private:
	void init();
	void createSwapChain();
	void cleanupSwapChain();
	void createImageViews();
	void registerImagesWithVramManager();
	VkPresentModeKHR getVulkanPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;

	const PhysicalDevice& m_physicalDevice;
	const LogicalDevice& m_logicalDevice;
	const Surface& m_surface;
	Window& m_window;
	Settings& m_settings;
	VramManager& m_vramManager;
	VkSwapchainKHR m_swapChain;

	std::vector<VkImage> m_swapChainImages;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	std::vector<VkImageView> m_swapChainImageViews;
	std::vector<VramHandle> m_imageHandles;
};