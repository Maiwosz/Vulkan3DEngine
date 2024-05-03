#include "Buffer.h"
#include "Renderer.h"
#include "Image.h"

Buffer::Buffer(Renderer* renderer) : m_renderer(renderer)
{
}

Buffer::~Buffer()
{
    unmap();
    vkDestroyBuffer(m_renderer->m_device->get(), m_buffer, nullptr);
    vkFreeMemory(m_renderer->m_device->get(), m_bufferMemory, nullptr);
}

void Buffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_renderer->m_device->get(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_renderer->m_device->get(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_renderer->m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_renderer->m_device->get(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(m_renderer->m_device->get(), buffer, bufferMemory, 0);
}

void Buffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = m_renderer->m_device->beginSingleTimeCommands();
    
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
    m_renderer->m_device->endSingleTimeCommands(commandBuffer);
}

void Buffer::copyBufferToImage(ImagePtr image)
{
    VkCommandBuffer commandBuffer = m_renderer->m_device->beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { image->getWidth(),image->getHeight(),1};

    vkCmdCopyBufferToImage(commandBuffer, m_buffer, image->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    image->updateLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    m_renderer->m_device->endSingleTimeCommands(commandBuffer);
}

VkResult Buffer::map(VkDeviceSize size, VkDeviceSize offset)
{
    return vkMapMemory(m_renderer->m_device->get(), m_bufferMemory, offset, size, 0, &m_mapped);
}

void Buffer::unmap()
{
    if (m_mapped) {
        vkUnmapMemory(m_renderer->m_device->get(), m_bufferMemory);
        m_mapped = nullptr;
    }
}
