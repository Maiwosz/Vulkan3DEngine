#include "FrameManager.h"

FrameManager::FrameManager(
    VulkanContext& vulkancontext,
    SynchronizationResourceManager& syncManager,
    CommandBufferManager& cmdBufferManager,
    uint32_t maxFrames
) : m_vulkanContext(vulkancontext),
m_syncManager(syncManager),
m_cmdBufferManager(cmdBufferManager),
m_maxFrames(maxFrames)
{
    m_frames.resize(m_maxFrames);

    CommandBufferManager::Configuration graphicsConfig{
        LogicalDevice::QueueType::Graphics,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    CommandBufferManager::Configuration transferConfig{
        LogicalDevice::QueueType::Transfer,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    CommandBufferManager::Configuration imguiConfig{
        LogicalDevice::QueueType::Graphics,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    for (uint32_t i = 0; i < m_maxFrames; ++i) {
        auto& frame = m_frames[i];

        // Acquire synchronization resources
        frame.imageAvailable = m_syncManager.acquireSemaphore();
        frame.renderFinished = m_syncManager.acquireSemaphore();
        frame.transferFinished = m_syncManager.acquireSemaphore();
        frame.inFlightFence = m_syncManager.acquireFence(true); // Fence starts signaled

        // Acquire command buffers
        frame.graphicsCommandBuffer = m_cmdBufferManager.acquireSmartBuffer(graphicsConfig);
        frame.transferCommandBuffer = m_cmdBufferManager.acquireSmartBuffer(transferConfig);
        frame.imguiCommandBuffer = m_cmdBufferManager.acquireSmartBuffer(imguiConfig);

        // Begin command buffers
        frame.graphicsCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        frame.transferCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        // ImGui command buffer zostanie rozpoczęty gdy będzie potrzebny
        frame.hasTransferCommands = false;
    }
}

FrameManager::~FrameManager() {
    for (auto& frame : m_frames) {
        // Smart handles will automatically release command buffers

        // Release synchronization resources
        m_syncManager.releaseSemaphore(frame.imageAvailable);
        m_syncManager.releaseSemaphore(frame.renderFinished);
        m_syncManager.releaseSemaphore(frame.transferFinished); // Fixed: was missing this line
        m_syncManager.releaseFence(frame.inFlightFence);
    }
}

void FrameManager::advanceFrame() {
    // Wait for the next frame to be ready before using it
    uint32_t nextFrame = (m_currentFrame + 1) % m_maxFrames;
    auto& frame = m_frames[nextFrame];

    // Wait for this frame's fence (ensure GPU finished with it)
    vkWaitForFences(
        m_vulkanContext.logical().get(),
        1,
        &frame.inFlightFence,
        VK_TRUE,
        UINT64_MAX
    );

    // Clear frame data
    frame.renderOrders.clear();
    frame.hasTransferCommands = false;

    // Advance to next frame
    m_currentFrame = nextFrame;
}

void FrameManager::clearCurrentFrameOrders()
{
    m_frames[m_currentFrame].renderOrders.clear();
    m_frames[m_currentFrame].hasTransferCommands = false;
}

void FrameManager::waitForAllFrames() {
    // First, wait for all queues to be idle to avoid any pending submissions
    vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics));
    vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Transfer));

    // Collect all fences that are not VK_NULL_HANDLE and not already signaled
    std::vector<VkFence> activeFences;
    activeFences.reserve(m_maxFrames);

    for (const auto& frame : m_frames) {
        if (frame.inFlightFence != VK_NULL_HANDLE) {
            // Check if fence is already signaled to avoid unnecessary waiting
            VkResult fenceStatus = vkGetFenceStatus(m_vulkanContext.logical().get(), frame.inFlightFence);
            if (fenceStatus == VK_NOT_READY) {
                activeFences.push_back(frame.inFlightFence);
            }
        }
    }

    // If we have active unsignaled fences, wait for them with a reasonable timeout
    if (!activeFences.empty()) {
        SPDLOG_INFO("Waiting for {} active fences during cleanup", activeFences.size());

        // Use a reasonable timeout instead of infinite wait to prevent deadlock
        constexpr uint64_t FENCE_TIMEOUT = 5000000000ULL; // 5 seconds in nanoseconds

        VkResult result = vkWaitForFences(
            m_vulkanContext.logical().get(),
            static_cast<uint32_t>(activeFences.size()),
            activeFences.data(),
            VK_TRUE,  // waitAll = true - wait for all fences
            FENCE_TIMEOUT
        );

        if (result == VK_TIMEOUT) {
            SPDLOG_WARN("Timeout waiting for fences during cleanup - forcing device idle");
            // Force device idle as last resort
            vkDeviceWaitIdle(m_vulkanContext.logical().get());
        }
        else if (result != VK_SUCCESS) {
            SPDLOG_ERROR("Failed to wait for frame fences during cleanup: {}", static_cast<int>(result));
            // Still try device idle to clean up
            vkDeviceWaitIdle(m_vulkanContext.logical().get());
        }
        else {
            SPDLOG_INFO("Successfully waited for all fences");
        }
    }
}

void FrameManager::resetFrame(uint32_t frameIndex) {
    if (frameIndex >= m_maxFrames) {
        SPDLOG_ERROR("Invalid frame index: {}", frameIndex);
        return;
    }

    auto& frame = m_frames[frameIndex];

    if (frame.inFlightFence != VK_NULL_HANDLE) {
        VkResult result = vkWaitForFences(
            m_vulkanContext.logical().get(),
            1,
            &frame.inFlightFence,
            VK_TRUE,
            UINT64_MAX
        );
        if (result != VK_SUCCESS) {
            SPDLOG_ERROR("Failed to wait for fence in resetFrame");
        }
    }

    // Clear render orders
    frame.renderOrders.clear();
    frame.hasTransferCommands = false;

    // Reset and restart command buffers if they exist
    if (frame.graphicsCommandBuffer) {
        if (frame.graphicsCommandBuffer->isRecording()) {
            frame.graphicsCommandBuffer->end();
        }
        frame.graphicsCommandBuffer->reset();
        frame.graphicsCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    }

    if (frame.transferCommandBuffer) {
        if (frame.transferCommandBuffer->isRecording()) {
            frame.transferCommandBuffer->end();
        }
        frame.transferCommandBuffer->reset();
        frame.transferCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    }

    // Reset ImGui command buffer
    if (frame.imguiCommandBuffer) {
        if (frame.imguiCommandBuffer->isRecording()) {
            frame.imguiCommandBuffer->end();
        }
        frame.imguiCommandBuffer->reset();
        frame.transferCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    }
}
