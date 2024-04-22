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

	//Tworzenie zasobów przenieœæ do osobnej klasy
	StagingBufferPtr createStagingBuffer(VkDeviceSize bufferSize);
	VertexBufferPtr createVertexBuffer(std::vector<Vertex> vertices);
	IndexBufferPtr createIndexBuffer(std::vector<uint32_t> indices);
	UniformBufferPtr createUniformBuffer(VkDeviceSize deviceSize);
	ImagePtr createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	ImageViewPtr createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	TextureSamplerPtr createTextureSampler(uint32_t mipLevels);
	TextureDescriptorSetPtr createTextureDescriptorSet(VkImageView imageView, VkSampler sampler);
	TransformDescriptorSetPtr createTransformDescriptorSet(VkBuffer uniformBuffer);
	GlobalDescriptorSetPtr createGlobalDescriptorSet(VkBuffer uniformBuffer);

	DevicePtr getDevice() { return m_device; }
	SwapChainPtr getSwapChain() { return m_swapChain; }

	void drawFrameBegin();
	void drawFrameEnd();
	//Wszystkie globalne ustawienie przenieœæ potem gdzie indziej
	static VkSampleCountFlagBits msaaSamples;
	static const int MAX_FRAMES_IN_FLIGHT = 2;

	VkCommandBuffer getCurrentCommandBuffer() { return m_commandBuffers[m_currentFrame]; }
	const uint32_t getCurrentFrame() { return m_currentFrame; }
	void bindDescriptorSets();

private:
	//Command Buffer // To separate class later?
	void createCommandBuffers();
	void recordCommandBufferBegin(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void recordCommandBufferEnd(VkCommandBuffer commandBuffer);
	
	WindowPtr m_window;
	DevicePtr m_device;
	SwapChainPtr m_swapChain;
	GraphicsPipelinePtr m_graphicsPipeline;
	DescriptorPoolPtr m_descriptorPool;
	GlobalDescriptorSetLayoutPtr m_globalDescriptorSetLayout;
	TextureDescriptorSetLayoutPtr m_textureDescriptorSetLayout;
	TransformDescriptorSetLayoutPtr m_transformDescriptorSetLayout;

	//Command Buffer // To separate class later?
	std::vector<VkCommandBuffer> m_commandBuffers;

	VkDescriptorSet m_currentDescriptorSets[3];

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
	friend class TransformDescriptorSetLayout;
	friend class DescriptorSet;
	friend class GlobalDescriptorSet;
	friend class TextureDescriptorSet;
	friend class TransformDescriptorSet;
	friend class Image;
	friend class ImageView;
	friend class TextureSampler;
	friend class DepthBuffer;
};

