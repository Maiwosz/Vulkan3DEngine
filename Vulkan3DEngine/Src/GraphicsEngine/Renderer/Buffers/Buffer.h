#pragma once
#include "..\..\..\Prerequisites.h"

class Buffer
{
public:
    Buffer(Renderer* renderer);
    ~Buffer();

    VkBuffer& get() { return m_buffer; }
    VkDeviceMemory& getMemory() { return m_bufferMemory; }
    void* getMappedMemory() { return m_mapped; }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void copyBufferToImage(ImagePtr image);
    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void unmap();

    virtual void bind() = 0; // Pure virtual function
protected:
    Renderer* m_renderer = nullptr;
    VkBuffer m_buffer;
    VkDeviceMemory m_bufferMemory;
    void* m_mapped = nullptr;
};

