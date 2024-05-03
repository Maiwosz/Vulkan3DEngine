#include "UniformBuffer.h"
#include "GraphicsEngine.h"

UniformBuffer::UniformBuffer(VkDeviceSize bufferSize, Renderer* renderer) : Buffer(renderer)
{
   // VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_buffer, m_bufferMemory);

    vkMapMemory(m_renderer->m_device->get(), m_bufferMemory, 0, bufferSize, 0, &m_mapped);
}

UniformBuffer::~UniformBuffer()
{
}

void UniformBuffer::bind()
{
}
