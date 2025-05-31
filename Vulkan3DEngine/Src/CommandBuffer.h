#pragma once
#include "CommandPool.h"
#include "Prerequisites.h"
#include <memory>

class CommandPool;

class CommandBuffer {
public:
    explicit CommandBuffer(std::shared_ptr<CommandPool> pool,
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    ~CommandBuffer();

    // Non-copyable, movable
    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&& other) noexcept;
    CommandBuffer& operator=(CommandBuffer&& other) noexcept;

    // Accessors
    [[nodiscard]] VkCommandBuffer handle() const noexcept { return m_commandBuffer; }
    [[nodiscard]] bool isRecording() const noexcept { return m_isRecording; }
    [[nodiscard]] VkCommandBufferLevel level() const noexcept { return m_level; }
    [[nodiscard]] uint32_t queueFamilyIndex() const noexcept;
    [[nodiscard]] bool isValid() const noexcept { return m_commandBuffer != VK_NULL_HANDLE; }

    // Recording control
    void begin(VkCommandBufferUsageFlags flags = 0);
    void end();
    void reset(VkCommandBufferResetFlags flags = 0);

    // Submission
    void submit(VkQueue queue,
        std::span<const VkSemaphore> waitSemaphores = {},
        std::span<const VkPipelineStageFlags> waitStages = {},
        std::span<const VkSemaphore> signalSemaphores = {},
        VkFence fence = VK_NULL_HANDLE);

    // Implicit conversion for Vulkan API calls
    operator VkCommandBuffer() const noexcept { return m_commandBuffer; }

private:
    void cleanup() noexcept;
    void safeEndRecording() noexcept;

    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    std::shared_ptr<CommandPool> m_pool;
    VkCommandBufferLevel m_level;
    bool m_isRecording = false;
};