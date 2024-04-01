#pragma once
#include "../../Prerequisites.h"
#include "Device/Device.h"
#include "SwapChain/SwapChain.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "DescriptorSet/DescriptorSet.h"

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

	VkCommandBuffer getCurrentCommandBuffer() { return m_commandBuffers[m_currentFrame]; }
private:
	//Command Buffer // To separate class later?
	void createCommandBuffers();
	void recordCommandBufferBegin(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void recordCommandBufferEnd(VkCommandBuffer commandBuffer);

	//Uniform Buffers // To separate class later?
	void createUniformBuffers();
	void updateUniformBuffer(uint32_t currentImage);
	void createDescriptorPool();
	void createDescriptorSets();

	WindowPtr m_window;
	DevicePtr m_device;
	SwapChainPtr m_swapChain;
	DescriptorSetLayoutPtr m_descriptorSetLayout;
	GraphicsPipelinePtr m_graphicsPipeline;

	//Command Buffer // To separate class later?
	std::vector<VkCommandBuffer> m_commandBuffers;
	
	//Uniform Buffers // To separate class later?
	std::vector<UniformBufferPtr> m_uniformBuffers;

	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;


	uint32_t m_currentFrame = 0;
	uint32_t m_currentImageIndex;

	friend class SwapChain;
	friend class GraphicsPipeline;
	friend class Device;
	friend class Buffer;
	friend class VertexBuffer;
	friend class IndexBuffer;
	friend class UniformBuffer;
	friend class DescriptorSetLayout;
	friend class DescriptorSet;
};

