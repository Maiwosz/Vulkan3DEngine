#pragma once
#include "..\..\..\Prerequisites.h"
#include "..\..\GraphicsEngine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Buffer
{
public:
    Buffer(Renderer* renderer);
    ~Buffer();

    virtual void bind() = 0; // Pure virtual function

protected:
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    Renderer* m_renderer = nullptr;
    VkBuffer m_buffer;
    VkDeviceMemory m_bufferMemory;
};

