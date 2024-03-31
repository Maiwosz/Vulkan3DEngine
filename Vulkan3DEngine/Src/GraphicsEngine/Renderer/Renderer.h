#pragma once
#include "../../Prerequisites.h"
#include "Device/Device.h"
#include "SwapChain/SwapChain.h"
#include "GraphicsPipeline/GraphicsPipeline.h"

class Renderer
{
public:
	Renderer(WindowPtr window);
	~Renderer();

	VertexBufferPtr createVertexBuffer(std::vector<Vertex> vertices);
	IndexBufferPtr createIndexBuffer(std::vector<uint16_t> indices);

	DevicePtr getDevice() { return m_device; }

	void drawFrameBegin();
	void drawFrameEnd();
	const int MAX_FRAMES_IN_FLIGHT = 2;

	VkCommandBuffer getCurrentCommandBuffer() { return commandBuffers[m_currentFrame]; }
private:
	//Command Buffer
	void createCommandBuffers();
	void recordCommandBufferBegin(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void recordCommandBufferEnd(VkCommandBuffer commandBuffer);

	WindowPtr m_window;
	DevicePtr m_device;
	SwapChainPtr m_swapChain;
	GraphicsPipelinePtr m_graphicsPipeline;
	std::vector<VkCommandBuffer> commandBuffers;
	
	uint32_t m_currentFrame = 0;
	uint32_t m_currentImageIndex;

	friend class SwapChain;
	friend class GraphicsPipeline;
	friend class Device;
	friend class Buffer;
	friend class VertexBuffer;
	friend class IndexBuffer;
};

