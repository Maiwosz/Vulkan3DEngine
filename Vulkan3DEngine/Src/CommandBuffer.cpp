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
        // FIX: End command buffer if it's in recording state before destruction
        if (m_isRecording) {
            try {
                end();
            }
            catch (const std::exception& e) {
                // Just log the error, we can't throw in a destructor
                // Using printf since spdlog might not be available
                fprintf(stderr, "Error ending command buffer in destructor: %s\n", e.what());
            }
        }
        vkFreeCommandBuffers(p_pool->getDevice(), p_pool->get(), 1, &m_commandBuffer);
    }
}

CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
    : m_commandBuffer(other.m_commandBuffer),
    p_pool(other.p_pool),
    m_isRecording(other.m_isRecording) {  // FIX: Copy isRecording flag
    other.m_commandBuffer = VK_NULL_HANDLE;
    other.m_isRecording = false;  // FIX: Reset isRecording flag
}

CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept {
    if (this != &other) {
        if (m_commandBuffer != VK_NULL_HANDLE) {
            // FIX: End command buffer if it's in recording state before destruction
            if (m_isRecording) {
                try {
                    end();
                }
                catch (const std::exception& e) {
                    // Just log the error, we can't throw in a move assignment
                    fprintf(stderr, "Error ending command buffer in move assignment: %s\n", e.what());
                }
            }
            vkFreeCommandBuffers(p_pool->getDevice(), p_pool->get(), 1, &m_commandBuffer);
        }
        m_commandBuffer = other.m_commandBuffer;
        p_pool = other.p_pool;
        m_isRecording = other.m_isRecording;  // FIX: Copy isRecording flag
        other.m_commandBuffer = VK_NULL_HANDLE;
        other.m_isRecording = false;  // FIX: Reset isRecording flag
    }
    return *this;
}

void CommandBuffer::begin(VkCommandBufferUsageFlags flags) {
    if (m_isRecording) {
        // FIX: Just log warning instead of throwing
        fprintf(stderr, "Warning: Command buffer is already in recording state\n");
        return;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;

    VkResult result = vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer: " + std::to_string(result));
    }

    m_isRecording = true;
}

void CommandBuffer::end() {
    if (!m_isRecording) {
        return; // Already ended, no need to throw
    }

    VkResult result = vkEndCommandBuffer(m_commandBuffer);
    if (result != VK_SUCCESS) {
        m_isRecording = false; // FIX: Reset recording state even on error
        throw std::runtime_error("Failed to end recording command buffer: " + std::to_string(result));
    }

    m_isRecording = false;
}

void CommandBuffer::submit(
    VkQueue queue,
    const std::vector<VkSemaphore>& waitSemaphores,
    const std::vector<VkPipelineStageFlags>& waitStages,
    const std::vector<VkSemaphore>& signalSemaphores,
    VkFence fence
) {
    // FIX: Remove this check to allow submit without recording
    if (m_isRecording) {
        // End the command buffer if it's still recording
        end();
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Configure wait semaphores and stages
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo.pWaitSemaphores = waitSemaphores.empty() ? nullptr : waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.empty() ? nullptr : waitStages.data();

    // Configure command buffers
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;

    // Configure signal semaphores
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    submitInfo.pSignalSemaphores = signalSemaphores.empty() ? nullptr : signalSemaphores.data();

    // Submit to queue
    VkResult result = vkQueueSubmit(queue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer to queue: " + std::to_string(result));
    }
}

void CommandBuffer::reset(VkCommandBufferResetFlags flags) {
    if (m_isRecording) {
        // FIX: End the command buffer if it's still recording
        try {
            end();
        }
        catch (const std::exception& e) {
            // Just log and continue
            fprintf(stderr, "Error ending command buffer before reset: %s\n", e.what());
        }
    }

    VkResult result = vkResetCommandBuffer(m_commandBuffer, flags);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to reset command buffer: " + std::to_string(result));
    }
}