#include "FrameManager.h"

FrameManager::FrameManager(
    SynchronizationResourceManager& syncManager,
    CommandBufferManager& cmdBufferManager,
    uint32_t maxFrames
) : m_syncManager(syncManager), m_cmdBufferManager(cmdBufferManager), m_maxFrames(maxFrames) {
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

    for (uint32_t i = 0; i < m_maxFrames; ++i) {
        auto& frame = m_frames[i];

        // Acquire synchronization resources
        frame.imageAvailable = m_syncManager.acquireSemaphore();
        frame.renderFinished = m_syncManager.acquireSemaphore();
        frame.inFlightFence = m_syncManager.acquireFence(true); // Fence starts signaled

        // Acquire command buffers
        frame.graphicsCommandBuffer = m_cmdBufferManager.acquireBuffer(graphicsConfig);
        frame.transferCommandBuffer = m_cmdBufferManager.acquireBuffer(transferConfig);
    }
}

FrameManager::~FrameManager() {
    for (auto& frame : m_frames) {
        // Release command buffers
        if (frame.graphicsCommandBuffer) {
            m_cmdBufferManager.releaseBuffer(std::move(frame.graphicsCommandBuffer));
        }
        if (frame.transferCommandBuffer) {
            m_cmdBufferManager.releaseBuffer(std::move(frame.transferCommandBuffer));
        }

        // Release synchronization resources
        m_syncManager.releaseSemaphore(frame.imageAvailable);
        m_syncManager.releaseSemaphore(frame.renderFinished);
        m_syncManager.releaseFence(frame.inFlightFence);
    }
}