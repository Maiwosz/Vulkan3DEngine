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
#include "DescriptorSetGuard.h"
#include <memory>
#include <vector>

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
        CommandBufferManager& cmdBufferManager,
        SynchronizationResourceManager& syncManager
    );
    ~Renderer();

    /**
     * Render a complete frame with the given render graph and GPU calls.
     */
    bool renderFrame(
        const SmartRenderGraphHandle& renderGraph,
        const std::vector<std::unique_ptr<GpuCall>>& gpuCalls
    );

    /**
     * Render an empty frame without any GPU calls.
     */
    bool renderEmptyFrame();

    // Global rendering state management
    bool bindPipeline(PipelineHandle pipelineHandle);

    /**
     * Bind descriptor sets with automatic GPU usage tracking.
     * Creates guards that will automatically release descriptors when GPU work completes.
     */
    bool bindDescriptorSets(const std::vector<DescriptorSetHandle>& descriptorHandles);

    /**
     * Bind descriptor sets using smart handles with automatic GPU usage tracking.
     */
    bool bindDescriptorSets(const std::vector<SmartHandle<DescriptorSetHandle, VkDescriptorSet>>& descriptorHandles);

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

    // Descriptor guard management
    void addDescriptorGuard(std::unique_ptr<DescriptorSetGuard> guard);
    size_t pollDescriptorGuards();

    // Renderer Sub Systems
    MeshRenderer& meshRenderer() { return *m_meshRenderer; }
    RenderGraphExecutor& renderGraphExecutor() { return *m_renderGraphExecutor; }

    std::shared_ptr<IImGuiProvider> getImGuiProvider() const { return m_renderGraphExecutor->getImGuiProvider(); }

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

    // Descriptor guard pool for automatic GPU usage tracking
    DescriptorSetGuardPool m_descriptorGuardPool;

    // Frame state
    bool m_frameActive = false;
    uint32_t m_currentImageIndex = 0;

    // Global rendering state
    PipelineHandle m_currentPipeline;
    VkPipelineLayout m_currentPipelineLayout = VK_NULL_HANDLE;

    // Current frame fence for descriptor tracking
    VkFence m_currentFrameFence = VK_NULL_HANDLE;

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
