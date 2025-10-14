#pragma once
#include "Prerequisites.h"
#include "RenderGraph.h"
#include "Handle.h"
#include "ISmartHandleManager.h"
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
class IImGuiProvider;

// Type aliases
using SmartRenderGraphHandle = SmartHandle<RenderGraphHandle, RenderGraph>;

/**
 * RenderGraphExecutor - interprets and executes RenderGraph structures
 *
 * Replaces rigid render pass usage in Renderer with flexible graph-based execution.
 * Takes a RenderGraph (which is pure structure/data) and executes all GpuCalls
 * across all nodes in sequence.
 *
 * Responsibilities:
 * - Traverse the render graph structure
 * - Acquire/cache framebuffers for each node (via FrameBufferManager)
 * - Manage render pass begin/end for each node
 * - Resolve attachment bindings (including swapchain images)
 * - Execute GpuCalls within appropriate render pass contexts
 * - Set viewport/scissor state per node
 * - Optionally inject ImGui rendering into the final render pass
 *
 * The executor uses existing managers for resource caching:
 * - FrameBufferManager caches framebuffers by configuration
 * - RenderPassManager caches render passes by configuration
 * - AttachmentManager caches attachments by specification
 *
 * ImGui Integration:
 * - ImGui provider can be optionally attached
 * - ImGui is always rendered in the final render pass
 * - Provider is automatically reinitialized when render pass changes
 * - ImGui remains lightweight and doesn't affect RenderGraph structure
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
     * Assign a render graph to be executed.
     * The graph describes structure only - executor handles all Vulkan operations.
     * SmartHandle automatically manages reference counting.
     *
     * @param renderGraphHandle Smart handle to the render graph, or invalid handle to clear assignment
     */
    void assignRenderGraph(const SmartRenderGraphHandle& renderGraphHandle);

    /**
     * Execute all GpuCalls on the assigned RenderGraph.
     *
     * For each node in the graph:
     * 1. Acquire framebuffer (cached by FrameBufferManager)
     * 2. Begin render pass with appropriate clear values
     * 3. Set viewport and scissor
     * 4. Execute all GpuCalls
     * 5. If final node and ImGui provider attached: render ImGui
     * 6. End render pass
     *
     * @param gpuCalls Vector of GpuCall commands to execute
     * @return true if execution succeeded, false otherwise
     */
    bool executeGpuCalls(const std::vector<std::unique_ptr<GpuCall>>& gpuCalls);

    /**
     * Check if a render graph is currently assigned
     */
    bool hasAssignedGraph() const { return m_assignedGraphHandle.isValid(); }

    /**
     * Get the currently assigned render graph (raw pointer for execution)
     */
    RenderGraph* getAssignedGraph() const { return m_assignedGraphHandle.get(); }

    /**
     * Get the smart handle to the currently assigned render graph
     */
    const SmartRenderGraphHandle& getAssignedGraphHandle() const { return m_assignedGraphHandle; }

    // ========== ImGui Integration ==========

    /**
     * Attach an ImGui provider for UI rendering.
     * Provider will be initialized for the final render pass.
     *
     * @param provider Shared pointer to ImGui provider (null to detach)
     */
    void attachImGuiProvider(std::shared_ptr<IImGuiProvider> provider);

    /**
     * Get the currently attached ImGui provider.
     *
     * @return Shared pointer to provider, or nullptr if none attached
     */
    std::shared_ptr<IImGuiProvider> getImGuiProvider() const { return m_imguiProvider; }

    /**
     * Check if an ImGui provider is attached.
     */
    bool hasImGuiProvider() const { return m_imguiProvider != nullptr; }

private:
    /**
     * Execute GpuCalls on a specific render node.
     * Handles framebuffer setup, render pass begin/end, and call execution.
     *
     * @param nodeIndex Index of the node in the graph
     * @param renderNode The node to execute on (provides template & render pass)
     * @param gpuCalls Vector of GpuCall commands
     * @param isFinalNode True if this is the final node in the graph
     * @return true if execution succeeded
     */
    bool executeNodeGpuCalls(
        size_t nodeIndex,
        RenderNode& renderNode,
        const std::vector<std::unique_ptr<GpuCall>>& gpuCalls,
        bool isFinalNode);

    /**
     * Setup framebuffer for a render node.
     * Queries the RenderGraph for attachment bindings and creates/acquires
     * a framebuffer from FrameBufferManager (which handles caching).
     *
     * @param nodeIndex Index of the node in the graph
     * @param renderNode The node to setup framebuffer for
     * @return FrameBufferHandle for the acquired framebuffer (cached)
     */
    FrameBufferHandle setupNodeFramebuffer(size_t nodeIndex, RenderNode& renderNode);

    /**
     * Begin render pass for a node.
     * Uses clear values from the graph and sets up viewport/scissor.
     *
     * @param nodeIndex Index of the node in the graph
     * @param renderNode The node to begin render pass for
     * @param framebufferHandle Handle to the framebuffer
     * @return true if render pass was successfully begun
     */
    bool beginNodeRenderPass(
        size_t nodeIndex,
        RenderNode& renderNode,
        FrameBufferHandle framebufferHandle);

    /**
     * End render pass for current node.
     * Safe to call multiple times - only ends if a pass is active.
     */
    void endNodeRenderPass();

    /**
     * Ensure ImGui provider is initialized for the final render pass.
     * Handles automatic reinitialization when render pass changes.
     *
     * @param finalNode The final render node
     * @return true if provider is ready for rendering
     */
    bool ensureImGuiProviderReady(RenderNode& finalNode);

    /**
     * Render ImGui UI in the current render pass.
     * Executes all registered callbacks and renders draw data.
     */
    void renderImGui();

    // Core systems
    EngineCore& m_engineCore;
    Renderer& m_renderer;

    // Resource managers (all handle caching internally)
    FrameBufferManager& m_framebufferManager;
    RenderPassManager& m_renderPassManager;
    SwapChain& m_swapChain;
    AttachmentManager& m_attachmentManager;
    PipelineManager& m_pipelineManager;

    // Current graph assignment - using SmartHandle for automatic lifetime management
    SmartRenderGraphHandle m_assignedGraphHandle;

    // Execution state
    bool m_nodeRenderPassActive = false;
    FrameBufferHandle m_currentNodeFramebuffer;

    // ImGui integration
    std::shared_ptr<IImGuiProvider> m_imguiProvider;
    bool m_imguiNeedsInit = true;
};