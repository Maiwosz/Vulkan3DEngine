#include "CommandBuffer.h"
#include <stdexcept>

CommandBuffer::CommandBuffer(CommandPool& pool, VkCommandBufferLevel level)
    : p_pool(&pool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = p_pool->get();
    allocInfo.level = level;
    allocInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(p_pool->getDevice(), &allocInfo, &m_commandBuffer));
}

CommandBuffer::~CommandBuffer() {
    if (m_commandBuffer) {
        vkFreeCommandBuffers(p_pool->getDevice(), p_pool->get(), 1, &m_commandBuffer);
    }
}

CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
    : m_commandBuffer(other.m_commandBuffer), p_pool(other.p_pool) {
    other.m_commandBuffer = VK_NULL_HANDLE;
}

CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept {
    if (this != &other) {
        if (m_commandBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(p_pool->getDevice(), p_pool->get(), 1, &m_commandBuffer);
        }
        m_commandBuffer = other.m_commandBuffer;
        p_pool = other.p_pool;
        other.m_commandBuffer = VK_NULL_HANDLE;
    }
    return *this;
}

void CommandBuffer::begin(VkCommandBufferUsageFlags flags) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;

    VK_CHECK(vkBeginCommandBuffer(m_commandBuffer, &beginInfo));
}

void CommandBuffer::end() {
    VK_CHECK(vkEndCommandBuffer(m_commandBuffer));
}

void CommandBuffer::submit(
    VkQueue queue,
    const std::vector<VkSemaphore>& waitSemaphores,
    const std::vector<VkPipelineStageFlags>& waitStages,
    const std::vector<VkSemaphore>& signalSemaphores,
    VkFence fence
) {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
}

void CommandBuffer::reset(VkCommandBufferResetFlags flags) {
    VK_CHECK(vkResetCommandBuffer(m_commandBuffer, flags));
}