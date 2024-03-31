#include "VertexBuffer.h"

VertexBuffer::VertexBuffer(std::vector<Vertex> vertices, Renderer* renderer) : Buffer(renderer)
{
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_renderer->m_device->get(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_renderer->m_device->get(), stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_buffer, m_bufferMemory);

    copyBuffer(stagingBuffer, m_buffer, bufferSize);
    vkDestroyBuffer(m_renderer->m_device->get(), stagingBuffer, nullptr);
    vkFreeMemory(m_renderer->m_device->get(), stagingBufferMemory, nullptr);
}

VertexBuffer::~VertexBuffer()
{
    vkDestroyBuffer(m_renderer->m_device->get(), m_buffer, nullptr);
    vkFreeMemory(m_renderer->m_device->get(), m_bufferMemory, nullptr);
}

void VertexBuffer::bind()
{
    VkBuffer buffers[] = { m_buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), 0, 1, buffers, offsets);
}