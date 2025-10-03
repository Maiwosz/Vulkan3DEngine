#pragma once
#include "Prerequisites.h"
#include "RenderGraph.h"
#include "Handle.h"
#include <vector>
#include <memory>

// Forward declarations
class EngineCore;
class Renderer;
class GpuCall;
class RenderNode;
class FrameBufferManager;
class AttachmentManager;
class RenderPassManager;
class SwapChain;
class PipelineManager;
class DrawCall;

/**
 * RenderGraphExecutor - interprets and executes RenderGraph structures
 *
 * Replaces rigid render pass usage in Renderer with flexible graph-based execution.
 * Takes a RenderGraph and executes all GpuCalls across all nodes in sequence.
 * Uses the existing Renderer for actual GPU command submission while handling
 * the graph traversal and render pass management internally.
 */
class RenderGraphExecutor {
public:
    explicit RenderGraphExecutor(EngineCore& engineCore, Renderer& renderer);
    ~RenderGraphExecutor() = default;

    // Non-copyable but movable
    RenderGraphExecutor(const RenderGraphExecutor&) = delete;
    RenderGraphExecutor& operator=(const RenderGraphExecutor&) = delete;
    RenderGraphExecutor(RenderGraphExecutor&&) = default;
    RenderGraphExecutor& operator=(RenderGraphExecutor&&) = default;

    /**
     * Assign a render graph to be executed
     * @param renderGraph Pointer to the render graph, or nullptr to clear assignment
     */
    void assignRenderGraph(RenderGraph* renderGraph);

    /**
     * Execute all GpuCalls on the assigned RenderGraph
     * @param gpuCalls Vector of GpuCall commands to execute
     * @return true if execution succeeded, false otherwise
     */
    bool executeGpuCalls(const std::vector<std::unique_ptr<GpuCall>>& gpuCalls);

    /**
     * Check if a render graph is currently assigned
     */
    bool hasAssignedGraph() const { return m_assignedGraph != nullptr; }

    /**
     * Get the currently assigned render graph
     */
    RenderGraph* getAssignedGraph() const { return m_assignedGraph; }

private:
    /**
     * Execute GpuCalls on a specific render node
     * @param renderNode The node to execute on
     * @param gpuCalls Vector of GpuCall commands
     * @return true if execution succeeded
     */
    bool executeNodeGpuCalls(RenderNode& renderNode,
        const std::vector<std::unique_ptr<GpuCall>>& gpuCalls);

    /**
     * Setup framebuffer for a render node
     * @param renderNode The node to setup framebuffer for
     * @return FrameBufferHandle for the created framebuffer
     */
    FrameBufferHandle setupNodeFramebuffer(RenderNode& renderNode);

    /**
     * Begin render pass for a node using RenderNode's own management
     * @param renderNode The node to begin render pass for
     * @param framebufferHandle Handle to the framebuffer
     */
    void beginNodeRenderPass(RenderNode& renderNode, FrameBufferHandle framebufferHandle);

    /**
     * End render pass for a node using RenderNode's own management
     * @param renderNode The node to end render pass for
     */
    void endNodeRenderPass(RenderNode& renderNode);

    /**
     * End render pass (fallback for exception cleanup)
     */
    void endNodeRenderPass();

    /**
     * Get or create pipeline for DrawCall with render pass information
     * @param drawCall The DrawCall containing pipeline configuration
     * @param renderPassHandle Handle to the current render pass
     * @param subpass Current subpass index
     * @return Pipeline handle for the configured pipeline
     */
    PipelineHandle getOrCreatePipeline(const DrawCall& drawCall,
        RenderPassHandle renderPassHandle,
        uint32_t subpass = 0);

    /**
     * Get extent for render target
     * @param renderTarget The render target to get extent for
     * @return VkExtent2D of the target
     */
    VkExtent2D getRenderTargetExtent(const RenderTarget& renderTarget);

    /**
     * Resolve attachment handles, replacing swapchain references with actual handles
     */
    std::vector<AttachmentHandle> resolveNodeAttachments(
        const RenderNode& renderNode,
        uint32_t swapchainImageIndex);

    EngineCore& m_engineCore;
    Renderer& m_renderer;
    FrameBufferManager& m_framebufferManager;
    RenderPassManager& m_renderPassManager;
    SwapChain& m_swapChain;
    AttachmentManager& m_attachmentManager;
    PipelineManager& m_pipelineManager;

    RenderGraph* m_assignedGraph = nullptr;

    // Execution state
    bool m_nodeRenderPassActive = false;
    FrameBufferHandle m_currentNodeFramebuffer;
};