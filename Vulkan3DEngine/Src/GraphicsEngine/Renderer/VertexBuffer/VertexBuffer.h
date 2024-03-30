#pragma once
#include "..\..\..\Prerequisites.h"
#include "..\..\GraphicsEngine.h"
#include "..\..\Device\Device.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

class VertexBuffer
{
public:
	VertexBuffer(std::vector<Vertex> vertices);
	~VertexBuffer();
	
	void bind();
private:
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
		VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	VkBuffer m_buffer;
	VkDeviceMemory m_bufferMemory;
};

