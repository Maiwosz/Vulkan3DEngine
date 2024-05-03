#include "StagingBuffer.h"
#include "GraphicsEngine.h"

StagingBuffer::StagingBuffer(VkDeviceSize bufferSize, Renderer* renderer) : Buffer(renderer)
{
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_buffer, m_bufferMemory);
}

StagingBuffer::~StagingBuffer()
{
    
}

void StagingBuffer::bind()
{
    
}
