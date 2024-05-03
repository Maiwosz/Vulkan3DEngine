#include "IndexBuffer.h"
#include "GraphicsEngine.h"
#include "StagingBuffer.h"

IndexBuffer::IndexBuffer(std::vector<uint32_t> indices, Renderer* renderer) : Buffer(renderer)
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    StagingBufferPtr stagingBuffer = m_renderer->createStagingBuffer(bufferSize);

    stagingBuffer->map();
    memcpy(stagingBuffer->getMappedMemory(), indices.data(), (size_t)bufferSize);
    stagingBuffer->unmap();

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_buffer, m_bufferMemory);

    copyBuffer(stagingBuffer->get(), m_buffer, bufferSize);
}

IndexBuffer::~IndexBuffer()
{
    
}

void IndexBuffer::bind()
{
    vkCmdBindIndexBuffer(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), m_buffer, 0, VK_INDEX_TYPE_UINT32);
}
