#include "CommandBuffer.h"
#include <stdexcept>
#include <format>

CommandBuffer::CommandBuffer(std::shared_ptr<CommandPool> pool, VkCommandBufferLevel level)
    : m_pool(std::move(pool)), m_level(level) {

    if (!m_pool) {
        throw std::invalid_argument("CommandPool cannot be null");
    }

    const VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_pool->get(),
        .level = level,
        .commandBufferCount = 1
    };

    if (const auto result = vkAllocateCommandBuffers(m_pool->getDevice(), &allocInfo, &m_commandBuffer);
        result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to allocate command buffer: {}", static_cast<int>(result)));
    }
}

CommandBuffer::~CommandBuffer() {
    cleanup();
}

CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
    : m_commandBuffer(std::exchange(other.m_commandBuffer, VK_NULL_HANDLE)),
    m_pool(std::move(other.m_pool)),
    m_level(other.m_level),
    m_isRecording(std::exchange(other.m_isRecording, false)) {
}

CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept {
    if (this != &other) {
        cleanup();

        m_commandBuffer = std::exchange(other.m_commandBuffer, VK_NULL_HANDLE);
        m_pool = std::move(other.m_pool);
        m_level = other.m_level;
        m_isRecording = std::exchange(other.m_isRecording, false);
    }
    return *this;
}

uint32_t CommandBuffer::queueFamilyIndex() const noexcept {
    return m_pool ? m_pool->getQueueFamilyIndex() : UINT32_MAX;
}

void CommandBuffer::begin(VkCommandBufferUsageFlags flags) {
    if (m_isRecording) {
        return; // Already recording
    }

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags
    };

    if (const auto result = vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
        result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to begin command buffer recording: {}", static_cast<int>(result)));
    }

    m_isRecording = true;
}

void CommandBuffer::end() {
    if (!m_isRecording) {
        return;
    }

    if (const auto result = vkEndCommandBuffer(m_commandBuffer);
        result != VK_SUCCESS) {
        m_isRecording = false;
        throw std::runtime_error(std::format("Failed to end command buffer recording: {}", static_cast<int>(result)));
    }

    m_isRecording = false;
}

void CommandBuffer::reset(VkCommandBufferResetFlags flags) {
    safeEndRecording();

    if (m_commandBuffer != VK_NULL_HANDLE) {
        if (const auto result = vkResetCommandBuffer(m_commandBuffer, flags);
            result != VK_SUCCESS) {
            throw std::runtime_error(std::format("Failed to reset command buffer: {}", static_cast<int>(result)));
        }
    }
}

void CommandBuffer::submit(VkQueue queue,
    std::span<const VkSemaphore> waitSemaphores,
    std::span<const VkPipelineStageFlags> waitStages,
    std::span<const VkSemaphore> signalSemaphores,
    VkFence fence) {
    if (m_isRecording) {
        end();
    }
    if (waitSemaphores.size() != waitStages.size()) {
        throw std::invalid_argument("Wait semaphores and wait stages must have the same size");
    }
    const VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
        .pWaitSemaphores = waitSemaphores.data(),
        .pWaitDstStageMask = waitStages.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &m_commandBuffer,
        .signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
        .pSignalSemaphores = signalSemaphores.data()
    };
    if (const auto result = vkQueueSubmit(queue, 1, &submitInfo, fence);
        result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to submit command buffer: {} ({})",
            string_VkResult(result), static_cast<int>(result)));
    }
}

void CommandBuffer::cleanup() noexcept {
    if (m_commandBuffer != VK_NULL_HANDLE && m_pool) {
        safeEndRecording();

        try {
            if (const auto result = vkResetCommandBuffer(m_commandBuffer, 0);
                result != VK_SUCCESS) {
                // Log warning but continue cleanup
            }
        }
        catch (...) {
            // Ignore exceptions during cleanup
        }

        vkFreeCommandBuffers(m_pool->getDevice(), m_pool->get(), 1, &m_commandBuffer);
        m_commandBuffer = VK_NULL_HANDLE;
    }

    m_pool.reset();
    m_isRecording = false;
}

void CommandBuffer::safeEndRecording() noexcept {
    if (m_isRecording && m_commandBuffer != VK_NULL_HANDLE) {
        try {
            vkEndCommandBuffer(m_commandBuffer);
        }
        catch (...) {
            // Ignore exceptions during cleanup
        }
        m_isRecording = false;
    }
}
