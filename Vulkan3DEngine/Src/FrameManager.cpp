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

        // Begin command buffers
        frame.graphicsCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        frame.transferCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        frame.hasTransferCommands = false;
    }
}

FrameManager::~FrameManager() {
    for (auto& frame : m_frames) {
		// Smart handles will automatically release command buffers

        // Release synchronization resources
        m_syncManager.releaseSemaphore(frame.imageAvailable);
        m_syncManager.releaseSemaphore(frame.renderFinished);
        m_syncManager.releaseFence(frame.inFlightFence);
    }
}

void FrameManager::clearCurrentFrameOrders()
{
    m_frames[m_currentFrame].renderOrders.clear();
    m_frames[m_currentFrame].hasTransferCommands = false;
}

void FrameManager::waitForAllFrames() {
    // Zbierz wszystkie fence'y które nie są VK_NULL_HANDLE
    std::vector<VkFence> activeFences;
    activeFences.reserve(m_maxFrames);

    for (const auto& frame : m_frames) {
        if (frame.inFlightFence != VK_NULL_HANDLE) {
            activeFences.push_back(frame.inFlightFence);
        }
    }

    // Jeśli mamy aktywne fence'y, czekaj na wszystkie naraz
    if (!activeFences.empty()) {
        VkResult result = vkWaitForFences(
            m_vulkanContext.logical().get(),
            static_cast<uint32_t>(activeFences.size()),
            activeFences.data(),
            VK_TRUE,  // waitAll = true - czekaj na wszystkie fence'y
            UINT64_MAX  // timeout - czekaj nieskończenie długo
        );

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to wait for frame fences during cleanup");
        }
    }
}