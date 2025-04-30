#pragma once
#include "SynchronizationResourceManager.h"
#include "CommandBufferManager.h"
#include <vector>
#include <memory>
#include "Buffer.h"

class Buffer;

struct FrameData {
    VkSemaphore imageAvailable;
    VkSemaphore renderFinished;
    VkFence inFlightFence;
    std::unique_ptr<CommandBuffer> graphicsCommandBuffer;
    std::unique_ptr<CommandBuffer> transferCommandBuffer;
    std::vector<Buffer> stagingBuffers;
};

class FrameManager {
public:
    FrameManager(
        SynchronizationResourceManager& syncManager,
        CommandBufferManager& cmdBufferManager,
        uint32_t maxFrames
    );
    ~FrameManager();

    FrameData& getCurrentFrame() { return m_frames[m_currentFrame]; }
    void advanceFrame() { m_currentFrame = (m_currentFrame + 1) % m_maxFrames; }

    VkSemaphore getImageAvailableSemaphore() const { return m_frames[m_currentFrame].imageAvailable; }
    VkSemaphore getRenderFinishedSemaphore() const { return m_frames[m_currentFrame].renderFinished; }
    VkFence getInFlightFence() const { return m_frames[m_currentFrame].inFlightFence; }

private:
    SynchronizationResourceManager& m_syncManager;
    CommandBufferManager& m_cmdBufferManager;
    std::vector<FrameData> m_frames;
    uint32_t m_currentFrame = 0;
    uint32_t m_maxFrames;
};