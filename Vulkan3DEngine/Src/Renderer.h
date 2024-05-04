#pragma once
#include "Prerequisites.h"
#include "Window.h"
#include "Device.h"
#include "SwapChain.h"
#include "PipelineBuilder.h"
//#include "DescriptorSet.h"
#include "Descriptors.h"

class Renderer
{
public:
	Renderer(WindowPtr window);
	~Renderer();

	//Tworzenie zasob�w przenie�� do osobnej klasy
	StagingBufferPtr createStagingBuffer(VkDeviceSize bufferSize);
	VertexBufferPtr createVertexBuffer(std::vector<Vertex> vertices);
	IndexBufferPtr createIndexBuffer(std::vector<uint32_t> indices);
	UniformBufferPtr createUniformBuffer(VkDeviceSize deviceSize);
	ImagePtr createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	ImageViewPtr createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	TextureSamplerPtr createTextureSampler(uint32_t mipLevels);

	DevicePtr getDevice() { return m_device; }
	SwapChainPtr getSwapChain() { return m_swapChain; }

	void drawFrameBegin();
	void drawFrameEnd();

	void bindDescriptorSet(VkDescriptorSet set, int position);

	VkCommandBuffer getCurrentCommandBuffer() { return m_commandBuffers[m_currentFrame]; }
	const uint32_t getCurrentFrame() { return m_currentFrame; }
	void bindDescriptorSets();

	//Settings
	static VkSampleCountFlagBits s_msaaSamples;
	static const int s_maxFramesInFlight = 2;

	VkDescriptorSetLayout m_globalDescriptorSetLayout;
	VkDescriptorSetLayout m_textureDescriptorSetLayout;
	VkDescriptorSetLayout m_modelDescriptorSetLayout;
private:
	//Command Buffer // To separate class later?
	void createCommandBuffers();
	void recordCommandBufferBegin(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void recordCommandBufferEnd(VkCommandBuffer commandBuffer);

	void createGraphicsPipeline();
	
	WindowPtr m_window;
	DevicePtr m_device;
	SwapChainPtr m_swapChain;
	PipelinePtr m_graphicsPipeline;
	DescriptorAllocatorGrowablePtr m_descriptorAllocator;

	//Command Buffer // To separate class later?
	std::vector<VkCommandBuffer> m_commandBuffers;

	VkDescriptorSet m_currentDescriptorSets[3];

	uint32_t m_currentFrame = 0;
	uint32_t m_currentImageIndex;

	friend class SwapChain;
	friend class GraphicsPipeline;
	friend class PipelineBuilder;
	friend class Device;
	friend class Buffer;
	friend class StagingBuffer;
	friend class VertexBuffer;
	friend class IndexBuffer;
	friend class UniformBuffer;
	friend class Image;
	friend class ImageView;
	friend class TextureSampler;
	friend class DepthBuffer;
};

