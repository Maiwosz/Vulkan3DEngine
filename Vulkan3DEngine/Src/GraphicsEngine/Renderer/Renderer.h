#pragma once
#include "../../Prerequisites.h"
#include "SwapChain/SwapChain.h"
#include "GraphicsPipeline/GraphicsPipeline.h"

class Renderer
{
public:
	Renderer(WindowPtr window, DevicePtr device);
	~Renderer();

	void drawFrameBegin();
	void drawFrameEnd();
	const int MAX_FRAMES_IN_FLIGHT = 2;

	VkCommandBuffer getCurrentCommandBuffer() { return commandBuffers[m_currentFrame]; }
private:
	//Command Buffer
	void createCommandBuffers();
	void recordCommandBufferBegin(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void recordCommandBufferEnd(VkCommandBuffer commandBuffer);

	DevicePtr m_device;
	SwapChainPtr m_swapChain;
	WindowPtr m_window;
	GraphicsPipelinePtr m_graphicsPipeline;
	std::vector<VkCommandBuffer> commandBuffers;
	
	uint32_t m_currentFrame = 0;
	uint32_t m_currentImageIndex;

	friend class SwapChain;
	friend class GraphicsPipeline;
};

