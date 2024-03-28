#pragma once
#include "../../Prerequisites.h"
#include "SwapChain/SwapChain.h"
#include "GraphicsPipeline/GraphicsPipeline.h"

class Renderer
{
public:
	Renderer(WindowPtr window, DevicePtr device);
	~Renderer();

	void drawFrame();
	const int MAX_FRAMES_IN_FLIGHT = 2;
private:
	//Command Buffer
	void createCommandBuffers();
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	DevicePtr m_device;
	SwapChainPtr m_swapChain;
	WindowPtr m_window;
	GraphicsPipelinePtr m_graphicsPipeline;
	std::vector<VkCommandBuffer> commandBuffers;

	
	uint32_t m_currentFrame = 0;

	friend class SwapChain;
	friend class GraphicsPipeline;
};

