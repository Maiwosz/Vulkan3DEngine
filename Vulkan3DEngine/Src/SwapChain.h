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
		VramManager& vramManager);
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

	// Event subscription for swap chain recreation
	auto onSwapChainRecreated(typename Event<>::Callback callback) { return m_swapChainRecreatedEvent->subscribe(callback); }

private:
	void init();
	void createSwapChain();
	void cleanupSwapChain();
	void createImageViews();
	void registerImagesWithVramManager();
	VkPresentModeKHR getVulkanPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;

	// Settings event handlers
	void onVsyncChanged(bool enabled);
	void onFramesInFlightChanged(uint32_t count);
	void onMsaaChanged(Settings::MsaaSampleCount sampleCount);

	const PhysicalDevice& r_physicalDevice;
	const LogicalDevice& r_logicalDevice;
	const Surface& r_surface;
	VramManager& r_vramManager;
	VkSwapchainKHR m_swapChain;

	std::vector<VkImage> m_swapChainImages;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	std::vector<VkImageView> m_swapChainImageViews;
	std::vector<VramHandle> m_imageHandles;

	// Event subscriptions
	std::unique_ptr<Event<bool>::Subscription> m_vsyncChangedSubscription;
	std::unique_ptr<Event<uint32_t>::Subscription> m_framesInFlightChangedSubscription;
	std::unique_ptr<Event<Settings::MsaaSampleCount>::Subscription> m_msaaChangedSubscription;

	// SwapChain recreated event
	std::shared_ptr<Event<>> m_swapChainRecreatedEvent;
};