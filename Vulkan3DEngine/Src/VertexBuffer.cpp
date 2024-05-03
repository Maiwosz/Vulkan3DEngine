#include "VertexBuffer.h"
#include "GraphicsEngine.h"
#include "StagingBuffer.h"

VertexBuffer::VertexBuffer(std::vector<Vertex> vertices, Renderer* renderer) : Buffer(renderer)
{
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    StagingBufferPtr stagingBuffer = m_renderer->createStagingBuffer(bufferSize);

    stagingBuffer->map();
    memcpy(stagingBuffer->getMappedMemory(), vertices.data(), (size_t)bufferSize);
    stagingBuffer->unmap();

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_buffer, m_bufferMemory);

    copyBuffer(stagingBuffer->get(), m_buffer, bufferSize);
}

VertexBuffer::~VertexBuffer()
{
}

void VertexBuffer::bind()
{
    VkBuffer buffers[] = { m_buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), 0, 1, buffers, offsets);
}