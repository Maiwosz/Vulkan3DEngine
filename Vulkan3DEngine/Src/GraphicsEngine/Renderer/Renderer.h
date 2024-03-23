#pragma once
#include "../../Prerequisites.h"
#include "SwapChain/SwapChain.h"
#include "GraphicsPipeline/GraphicsPipeline.h"

class Renderer
{
public:
	Renderer(Device& device);
	~Renderer();

	void drawFrame();
private:
	//Command Buffer
	void createCommandBuffer();
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	Device& m_device;
	SwapChainPtr m_swapChain;
	GraphicsPipelinePtr m_graphicsPipeline;
	VkCommandBuffer m_commandBuffer;
};

