#pragma once
#include "SynchronizationResourceManager.h"
#include "CommandBufferManager.h"
#include <vector>
#include <memory>
#include "RenderOrder.h"
#include "Handle.h"

class RenderOrder;

struct FrameData {
    VkSemaphore imageAvailable;
    VkSemaphore renderFinished;
    VkSemaphore transferFinished;
    VkFence inFlightFence;
    SmartCommandBufferHandle graphicsCommandBuffer;
    SmartCommandBufferHandle transferCommandBuffer;
    std::vector<std::shared_ptr<RenderOrder>> renderOrders;
    bool hasTransferCommands = false;
	FrameBufferHandle framebufferHandle;
};

class FrameManager {
public:
    FrameManager(
        VulkanContext& vulkancontext,
        SynchronizationResourceManager& syncManager,
        CommandBufferManager& cmdBufferManager,
        uint32_t maxFrames
    );
    ~FrameManager();

    FrameData& getCurrentFrame() { return m_frames[m_currentFrame]; }
    void advanceFrame() { m_currentFrame = (m_currentFrame + 1) % m_maxFrames; }

    void clearCurrentFrameOrders();

    void waitForAllFrames();

    // Add method to reset a specific frame
    void resetFrame(uint32_t frameIndex);

    // Add method to reset all frames
    void resetAllFrames() {
        for (uint32_t i = 0; i < m_maxFrames; ++i) {
            resetFrame(i);
        }
    }

    VkSemaphore getImageAvailableSemaphore() const { return m_frames[m_currentFrame].imageAvailable; }
    VkSemaphore getRenderFinishedSemaphore() const { return m_frames[m_currentFrame].renderFinished; }
    VkSemaphore gettransferFinishedSemaphore() const { return m_frames[m_currentFrame].transferFinished; }
    VkFence getInFlightFence() const { return m_frames[m_currentFrame].inFlightFence; }

    uint32_t getCurrentFrameIndex() { return m_currentFrame; }

private:
    VulkanContext& m_vulkanContext;
    SynchronizationResourceManager& m_syncManager;
    CommandBufferManager& m_cmdBufferManager;
    std::vector<FrameData> m_frames;
    uint32_t m_currentFrame = 0;
    uint32_t m_maxFrames;
};