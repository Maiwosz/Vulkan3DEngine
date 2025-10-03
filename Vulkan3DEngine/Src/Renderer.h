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
#include <memory>
#include <vector>

// Forward declarations
class GpuCall;
class MeshRenderer;
class UIRenderer;
class ImGuiWrapper;
class RenderGraph;

/**
 * Core Renderer - orchestrates frame lifecycle and provides GPU context
 * Simplified to focus on coordination rather than specific rendering operations
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
        ImGuiWrapper& imguiWrapper
    );
    ~Renderer();

    // Frame lifecycle - main responsibility
    bool beginFrame();
    void endFrame();

    // RenderGraph assignment
    void assignRenderGraph(RenderGraph* renderGraph);
    bool hasAssignedRenderGraph() const;
    RenderGraph* getAssignedRenderGraph() const;

    // GpuCall execution - simplified interface
    bool executeGpuCall(GpuCall& gpuCall);
    bool executeGpuCalls(const std::vector<std::unique_ptr<GpuCall>>& gpuCalls);

    // RenderGraph-based execution - NEW
    bool executeRenderGraph(const std::vector<std::unique_ptr<GpuCall>>& gpuCalls);

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
    UIRenderer& uiRenderer() { return *m_uiRenderer; }
    RenderGraphExecutor& renderGraphExecutor() { return *m_renderGraphExecutor; }

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
    ImGuiWrapper& m_imguiWrapper;

    // Service objects
    std::unique_ptr<MeshRenderer> m_meshRenderer;
    std::unique_ptr<UIRenderer> m_uiRenderer;
    std::unique_ptr<RenderGraphExecutor> m_renderGraphExecutor;

    // Frame state - SIMPLIFIED
    bool m_frameActive = false;
    uint32_t m_currentImageIndex = 0;

    // Global rendering state
    PipelineHandle m_currentPipeline;
    VkPipelineLayout m_currentPipelineLayout = VK_NULL_HANDLE;

    // Internal frame lifecycle methods
    void prepareFrame();
    uint32_t acquireSwapchainImage();
    void submitAndPresent();
    void cleanupFrame();
    void handleSwapchainRecreation();
    void ensureFrameActive() const;
};