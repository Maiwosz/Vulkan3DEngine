#pragma once
#include "CommandPool.h"
#include "Prerequisites.h"

class CommandPool;

class CommandBuffer {
public:
    CommandBuffer(CommandPool& pool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    ~CommandBuffer();

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;

    CommandBuffer(CommandBuffer&& other) noexcept;
    CommandBuffer& operator=(CommandBuffer&& other) noexcept;

	VkCommandBuffer get() const { return m_commandBuffer; }

    void begin(VkCommandBufferUsageFlags flags = 0);
    void end();
    void submit(
        VkQueue queue,
        const std::vector<VkSemaphore>& waitSemaphores = {},
        const std::vector<VkPipelineStageFlags>& waitStages = {},
        const std::vector<VkSemaphore>& signalSemaphores = {},
        VkFence fence = VK_NULL_HANDLE
    );
    void reset(VkCommandBufferResetFlags flags = 0);

    operator VkCommandBuffer() const { return m_commandBuffer; }
    uint32_t getQueueFamilyIndex() const { return p_pool->getQueueFamilyIndex(); }

private:
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    CommandPool* p_pool;
};