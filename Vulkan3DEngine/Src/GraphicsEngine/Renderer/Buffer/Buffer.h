#pragma once
#include "..\..\..\Prerequisites.h"
#include "..\..\GraphicsEngine.h"
#include "..\..\Device\Device.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

class Buffer
{
public:
	Buffer(std::vector<Vertex> vertices);
	~Buffer();
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
		VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void createVertexBuffer(std::vector<Vertex> vertices);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void bind(VkCommandBuffer commandBuffer);
private:
	VkBuffer m_buffer;
	VkDeviceMemory m_bufferMemory;
};

