#pragma once
#include "../../Prerequisites.h"
#include "../../WindowSytem/Window.h"
#include "Device/Device.h"
#include "SwapChain/SwapChain.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "DescriptorSets/DescriptorSet/DescriptorSet.h"

class Renderer
{
public:
	Renderer(WindowPtr window);
	~Renderer();

	StagingBufferPtr createStagingBuffer(VkDeviceSize bufferSize);
	VertexBufferPtr createVertexBuffer(std::vector<Vertex> vertices);
	IndexBufferPtr createIndexBuffer(std::vector<uint32_t> indices);
	ImagePtr createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	ImageViewPtr createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	TextureSamplerPtr createTextureSampler();
	TextureDescriptorSetPtr createTextureDescriptorSet(VkImageView imageView, VkSampler sampler);

	DevicePtr getDevice() { return m_device; }

	void drawFrameBegin();
	void drawFrameEnd();
	static const int MAX_FRAMES_IN_FLIGHT = 2;

	VkCommandBuffer getCurrentCommandBuffer() { return m_commandBuffers[m_currentFrame]; }
	const uint32_t getCurrentFrame() { return m_currentFrame; }

	void bindDescriptorSets();
private:
	//Command Buffer // To separate class later?
	void createCommandBuffers();
	void recordCommandBufferBegin(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void recordCommandBufferEnd(VkCommandBuffer commandBuffer);

	void createUniformBuffers();
	void updateUniformBuffer(uint32_t currentImage);

	WindowPtr m_window;
	DevicePtr m_device;
	SwapChainPtr m_swapChain;
	GraphicsPipelinePtr m_graphicsPipeline;
	TextureSamplerPtr m_textureSampler;
	DescriptorPoolPtr m_descriptorPool;
	GlobalDescriptorSetLayoutPtr m_globalDescriptorSetLayout;
	TextureDescriptorSetLayoutPtr m_textureDescriptorSetLayout;
	//DepthBufferPtr m_depthBuffer;

	//Command Buffer // To separate class later?
	std::vector<VkCommandBuffer> m_commandBuffers;
	
	std::vector<UniformBufferPtr> m_uniformBuffers;

	std::vector<DescriptorSetPtr> m_globalDescriptorSets;

	std::vector <VkDescriptorSet> m_currentDescriptorSets;

	uint32_t m_currentFrame = 0;
	uint32_t m_currentImageIndex;

	friend class SwapChain;
	friend class GraphicsPipeline;
	friend class Device;
	friend class Buffer;
	friend class StagingBuffer;
	friend class VertexBuffer;
	friend class IndexBuffer;
	friend class UniformBuffer;
	friend class DescriptorPool;
	friend class DescriptorSetLayout;
	friend class GlobalDescriptorSetLayout;
	friend class TextureDescriptorSetLayout;
	friend class DescriptorSet;
	friend class GlobalDescriptorSet;
	friend class TextureDescriptorSet;
	friend class Image;
	friend class ImageView;
	friend class TextureSampler;
	friend class DepthBuffer;
};

