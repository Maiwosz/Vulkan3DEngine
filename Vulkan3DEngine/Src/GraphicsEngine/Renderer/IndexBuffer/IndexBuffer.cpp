#include "IndexBuffer.h"

IndexBuffer::IndexBuffer(std::vector<uint16_t> indices)
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;

    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(GraphicsEngine::get()->getDevice()->get(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(GraphicsEngine::get()->getDevice()->get(), stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_buffer, m_bufferMemory);

    copyBuffer(stagingBuffer, m_buffer, bufferSize);

    vkDestroyBuffer(GraphicsEngine::get()->getDevice()->get(), stagingBuffer, nullptr);
    vkFreeMemory(GraphicsEngine::get()->getDevice()->get(), stagingBufferMemory, nullptr);
}

IndexBuffer::~IndexBuffer()
{
    vkDestroyBuffer(GraphicsEngine::get()->getDevice()->get(), m_buffer, nullptr);
    vkFreeMemory(GraphicsEngine::get()->getDevice()->get(), m_bufferMemory, nullptr);
}

void IndexBuffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(GraphicsEngine::get()->getDevice()->get(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(GraphicsEngine::get()->getDevice()->get(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = GraphicsEngine::get()->getDevice()->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(GraphicsEngine::get()->getDevice()->get(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(GraphicsEngine::get()->getDevice()->get(), buffer, bufferMemory, 0);
}

void IndexBuffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; 
    allocInfo.commandPool = GraphicsEngine::get()->getDevice()->getCommandPool();
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(GraphicsEngine::get()->getDevice()->get(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(GraphicsEngine::get()->getDevice()->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(GraphicsEngine::get()->getDevice()->getGraphicsQueue());
    vkFreeCommandBuffers(GraphicsEngine::get()->getDevice()->get(),
        GraphicsEngine::get()->getDevice()->getCommandPool(), 1, &commandBuffer);

}

void IndexBuffer::bind()
{
    vkCmdBindIndexBuffer(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), m_buffer, 0, VK_INDEX_TYPE_UINT16);
}
