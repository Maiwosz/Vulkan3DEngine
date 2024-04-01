#include "IndexBuffer.h"

IndexBuffer::IndexBuffer(std::vector<uint16_t> indices, Renderer* renderer) : Buffer(renderer)
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;

    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_renderer->m_device->get(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_renderer->m_device->get(), stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_buffer, m_bufferMemory);

    copyBuffer(stagingBuffer, m_buffer, bufferSize);

    vkDestroyBuffer(m_renderer->m_device->get(), stagingBuffer, nullptr);
    vkFreeMemory(m_renderer->m_device->get(), stagingBufferMemory, nullptr);
}

IndexBuffer::~IndexBuffer()
{
    
}

void IndexBuffer::bind()
{
    vkCmdBindIndexBuffer(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), m_buffer, 0, VK_INDEX_TYPE_UINT16);
}
