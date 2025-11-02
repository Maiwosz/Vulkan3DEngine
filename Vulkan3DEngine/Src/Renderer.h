#pragma once
#include "Prerequisites.h"
#include "EngineCore.h"
#include "VulkanContext.h"
#include "FrameManager.h"
#include "SwapChain.h"
#include "AttachmentManager.h"
#include "FrameBufferManager.h"
#include "RenderPassManager.h"
#include "VramManager.h"
#include "DescriptorAllocator.h"
#include "PipelineManager.h"
#include "RenderGraphExecutor.h"
#include "ISmartHandleManager.h"
#include <memory>
#include <vector>
#include "ComputeDispatcher.h"

// Forward declarations
class GpuCall;
class MeshRenderer;
class RenderGraph;

// Type aliases
using SmartRenderGraphHandle = SmartHandle<RenderGraphHandle, RenderGraph>;

/**
 * Core Renderer - orchestrates frame lifecycle and provides GPU context
 */
class Renderer {
public:
    explicit Renderer(
        EngineCore& engineCore,
        VulkanContext& vulkanContext,
        FrameManager& frameManager,
        VramManager& vramManager,
        SwapChain& swapChain,
        AttachmentManager& attachmentManager,
        FrameBufferManager& framebufferManager,
        RenderPassManager& renderPassManager,
        DescriptorAllocator& descriptorAllocator,
        PipelineManager& pipelineManager,
        CommandBufferManager& cmdBufferManager
    );
    ~Renderer();

    /**
     * Render a complete frame with the given render graph and GPU calls.
     * This is the primary rendering function - it handles:
     * - Frame initialization (beginFrame)
     * - RenderGraph assignment to executor
     * - GPU call execution through RenderGraphExecutor
     * - Frame finalization (endFrame)
     *
     * @param renderGraph The render graph defining render passes and attachments
     * @param gpuCalls Vector of GPU commands to execute (DrawCalls, etc.)
     * @return true if frame was rendered successfully, false on error
     *
     * Example usage:
     *   if (!renderer.renderFrame(cameraOrder.renderGraphHandle, drawCalls)) {
     *       SPDLOG_ERROR("Frame rendering failed");
     *   }
     */
    bool renderFrame(
        const SmartRenderGraphHandle& renderGraph,
        const std::vector<std::unique_ptr<GpuCall>>& gpuCalls
    );

    /**
     * Render an empty frame without any GPU calls.
     * Useful for maintaining frame pacing when there's nothing to render.
     *
     * @return true if frame was completed successfully
     */
    bool renderEmptyFrame();

    // Global rendering state management
    bool bindPipeline(PipelineHandle pipelineHandle);
    bool bindDescriptorSets(const std::vector<DescriptorSetHandle>& descriptorHandles);

    // State queries
    bool isFrameActive() const { return m_frameActive; }
    bool isPipelineBound() const { return m_currentPipeline.isValid(); }
    PipelineHandle getCurrentPipeline() const { return m_currentPipeline; }
    uint32_t getCurrentImageIndex() const { return m_currentImageIndex; }

    // Command buffer access
    VkCommandBuffer getCurrentCommandBuffer() const;
    VkCommandBuffer getTransferCommandBuffer() const;
    VkCommandBuffer getImGuiCommandBuffer() const;

    // Viewport management
    void setViewport(VkExtent2D extent, float minDepth = 0.0f, float maxDepth = 1.0f);
    void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height, float minDepth = 0.0f, float maxDepth = 1.0f);
    void setScissor(VkExtent2D extent, VkOffset2D offset = { 0, 0 });
    void setScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

    // Renderer Sub Systems
    MeshRenderer& meshRenderer() { return *m_meshRenderer; }
    RenderGraphExecutor& renderGraphExecutor() { return *m_renderGraphExecutor; }
    ComputeDispatcher& computeDispatcher() { return *m_computeDispatcher; }

private:
    // Engine references
    EngineCore& m_engineCore;
    VulkanContext& m_vulkanContext;
    FrameManager& m_frameManager;
    VramManager& m_vramManager;
    SwapChain& m_swapChain;
    AttachmentManager& m_attachmentManager;
    FrameBufferManager& m_framebufferManager;
    RenderPassManager& m_renderPassManager;
    DescriptorAllocator& m_descriptorAllocator;
    PipelineManager& m_pipelineManager;

    // Service objects
    std::unique_ptr<MeshRenderer> m_meshRenderer;
    std::unique_ptr<RenderGraphExecutor> m_renderGraphExecutor;
    std::unique_ptr<ComputeDispatcher> m_computeDispatcher;

    // Frame state
    bool m_frameActive = false;
    uint32_t m_currentImageIndex = 0;

    // Global rendering state
    PipelineHandle m_currentPipeline;
    VkPipelineLayout m_currentPipelineLayout = VK_NULL_HANDLE;

    // Internal frame lifecycle methods
    bool beginFrame();
    void endFrame();
    void prepareFrame();
    uint32_t acquireSwapchainImage();
    void submitAndPresent();
    void cleanupFrame();
    void handleSwapchainRecreation();
    void ensureFrameActive() const;
};