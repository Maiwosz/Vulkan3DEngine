#include "RenderGraphExecutor.h"
#include "Renderer.h"
#include "GpuCall.h"
#include "RenderNode.h"
#include "RenderTarget.h"
#include "FrameBufferManager.h"
#include "AttachmentManager.h"
#include "SwapChain.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

RenderGraphExecutor::RenderGraphExecutor(EngineCore& engineCore, Renderer& renderer)
    : m_engineCore(engineCore),
    m_renderer(renderer),
    m_framebufferManager(engineCore.framebufferManager()),
    m_renderPassManager(engineCore.renderPassManager()),
    m_swapChain(engineCore.swapChain()),
    m_attachmentManager(engineCore.attachmentManager()),
    m_pipelineManager(engineCore.pipelineManager()) {
}

void RenderGraphExecutor::assignRenderGraph(RenderGraph* renderGraph) {
    m_assignedGraph = renderGraph;

    if (renderGraph) {
        SPDLOG_DEBUG("RenderGraphExecutor: Assigned render graph with {} nodes",
            renderGraph->getNodeCount());
    }
    else {
        SPDLOG_DEBUG("RenderGraphExecutor: Cleared render graph assignment");
    }
}

void Renderer::assignRenderGraph(RenderGraph* renderGraph) {
    if (!m_renderGraphExecutor) {
        SPDLOG_ERROR("Renderer: RenderGraphExecutor not initialized");
        return;
    }

    m_renderGraphExecutor->assignRenderGraph(renderGraph);

    if (renderGraph) {
        SPDLOG_DEBUG("Renderer: Assigned render graph with {} nodes",
            renderGraph->getNodeCount());
    }
    else {
        SPDLOG_DEBUG("Renderer: Cleared render graph assignment");
    }
}

bool Renderer::hasAssignedRenderGraph() const {
    return m_renderGraphExecutor && m_renderGraphExecutor->hasAssignedGraph();
}

RenderGraph* Renderer::getAssignedRenderGraph() const {
    if (!m_renderGraphExecutor) {
        return nullptr;
    }
    return m_renderGraphExecutor->getAssignedGraph();
}

bool RenderGraphExecutor::executeGpuCalls(const std::vector<std::unique_ptr<GpuCall>>& gpuCalls) {
    if (!m_assignedGraph) {
        SPDLOG_ERROR("No render graph assigned - cannot execute GPU calls");
        return false;
    }

    if (!m_renderer.isFrameActive()) {
        SPDLOG_ERROR("No active frame - cannot execute GPU calls");
        return false;
    }

    if (gpuCalls.empty()) {
        SPDLOG_DEBUG("No GPU calls to execute");
        return true;
    }

    try {
        // Execute GPU calls on all nodes in sequence
        const auto& nodes = m_assignedGraph->getNodes();

        for (const auto& graphNode : nodes) {
            // Access RenderNode through smart handle
            if (!graphNode.renderNodeHandle.isValid()) {
                SPDLOG_ERROR("Invalid render node handle in graph");
                return false;
            }

            RenderNode* renderNode = graphNode.renderNodeHandle.get();
            if (!renderNode) {
                SPDLOG_ERROR("Failed to resolve render node from handle");
                return false;
            }

            if (!executeNodeGpuCalls(*renderNode, gpuCalls)) {
                SPDLOG_ERROR("Failed to execute GPU calls on node");
                return false;
            }
        }

        SPDLOG_DEBUG("Successfully executed {} GPU calls on {} nodes",
            gpuCalls.size(), nodes.size());
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during GPU call execution: {}", e.what());

        // Cleanup any active render pass
        if (m_nodeRenderPassActive) {
            endNodeRenderPass();
        }

        return false;
    }
}

bool RenderGraphExecutor::executeNodeGpuCalls(RenderNode& renderNode,
    const std::vector<std::unique_ptr<GpuCall>>& gpuCalls) {

    // Setup framebuffer for this node
    FrameBufferHandle framebufferHandle = setupNodeFramebuffer(renderNode);
    if (!framebufferHandle.isValid()) {
        SPDLOG_ERROR("Failed to setup framebuffer for render node");
        return false;
    }

    // Begin render pass for this node
    beginNodeRenderPass(renderNode, framebufferHandle);

    // Execute all GPU calls within this render pass
    // Each GpuCall now has access to the RenderNode context
    bool success = true;
    for (const auto& gpuCall : gpuCalls) {
        if (!gpuCall) {
            SPDLOG_ERROR("Null GPU call encountered");
            success = false;
            break;
        }

        // Execute the GPU call with render node context
        // DrawCalls will handle their own pipeline management
        if (!gpuCall->execute(m_renderer, m_engineCore, renderNode)) {
            SPDLOG_ERROR("Failed to execute GPU call");
            success = false;
            break;
        }
    }

    // End render pass for this node
    endNodeRenderPass(renderNode);

    return success;
}

FrameBufferHandle RenderGraphExecutor::setupNodeFramebuffer(RenderNode& renderNode) {
    if (!renderNode.isComplete()) {
        SPDLOG_ERROR("Render node is not complete - cannot create framebuffer");
        return FrameBufferHandle(0);
    }

    // Get attachments from render node
    std::vector<AttachmentHandle> attachmentHandles;

    // If node uses swapchain, we need current image index
    uint32_t swapchainImageIndex = 0;
    if (renderNode.usesSwapchainAttachment()) {
        swapchainImageIndex = m_renderer.getCurrentImageIndex();

        const auto& swapchainAttachments = m_swapChain.getAttachmentHandles();
        if (swapchainImageIndex >= swapchainAttachments.size()) {
            SPDLOG_ERROR("Invalid swapchain image index: {}", swapchainImageIndex);
            return FrameBufferHandle(0);
        }
    }

    // Collect all attachments in proper order
    attachmentHandles = resolveNodeAttachments(renderNode, swapchainImageIndex);

    if (attachmentHandles.empty()) {
        SPDLOG_ERROR("Render node has no attachments");
        return FrameBufferHandle(0);
    }

    // Get extent based on render target
    VkExtent2D extent = getRenderTargetExtent(renderNode.getRenderTarget());

    if (extent.width == 0 || extent.height == 0) {
        SPDLOG_ERROR("Invalid render target extent: {}x{}", extent.width, extent.height);
        return FrameBufferHandle(0);
    }

    // Create framebuffer configuration
    FrameBufferConfig config;
    config.renderPassHandle = renderNode.getRenderPassHandle();
    config.attachmentHandles = attachmentHandles;
    config.extent = extent;

    // Acquire framebuffer from manager
    return m_framebufferManager.acquireFrameBuffer(config);
}

void RenderGraphExecutor::beginNodeRenderPass(RenderNode& renderNode, FrameBufferHandle framebufferHandle) {
    if (m_nodeRenderPassActive) {
        SPDLOG_ERROR("Node render pass already active");
        throw std::runtime_error("Node render pass already active");
    }

    // Store current framebuffer
    m_currentNodeFramebuffer = framebufferHandle;

    // Get framebuffer resource
    auto* framebuffer = m_framebufferManager.getResource(framebufferHandle);
    if (!framebuffer) {
        throw std::runtime_error("Invalid framebuffer handle");
    }

    // Use RenderNode's render pass management
    renderNode.beginRenderPass(
        m_renderer.getCurrentCommandBuffer(),
        framebuffer->frameBuffer,
        framebuffer->config.extent
    );

    m_nodeRenderPassActive = true;

    // Set viewport and scissor for this framebuffer
    m_renderer.setViewport(framebuffer->config.extent);
    m_renderer.setScissor(framebuffer->config.extent);

    SPDLOG_DEBUG("Began render pass for node, extent: {}x{}",
        framebuffer->config.extent.width,
        framebuffer->config.extent.height);
}

void RenderGraphExecutor::endNodeRenderPass(RenderNode& renderNode) {
    if (!m_nodeRenderPassActive) {
        return;
    }

    renderNode.endRenderPass(m_renderer.getCurrentCommandBuffer());

    m_nodeRenderPassActive = false;
    m_currentNodeFramebuffer = FrameBufferHandle(0);

    SPDLOG_DEBUG("Ended render pass for node");
}

void RenderGraphExecutor::endNodeRenderPass() {
    if (!m_nodeRenderPassActive) {
        return;
    }

    // Fallback version for exception cleanup
    vkCmdEndRenderPass(m_renderer.getCurrentCommandBuffer());

    m_nodeRenderPassActive = false;
    m_currentNodeFramebuffer = FrameBufferHandle(0);

    SPDLOG_DEBUG("Ended render pass for node (fallback)");
}

VkExtent2D RenderGraphExecutor::getRenderTargetExtent(const RenderTarget& renderTarget) {
    if (renderTarget.isSwapchain()) {
        return m_swapChain.getSwapChainExtent();
    }
    else if (renderTarget.isTexture()) {
        const auto& textureHandle = renderTarget.getTextureHandle();
        if (textureHandle.isValid()) {
            auto* textureInfo = textureHandle.get();
            if (textureInfo) {
                return {
                    static_cast<uint32_t>(textureInfo->width),
                    static_cast<uint32_t>(textureInfo->height)
                };
            }
        }
        SPDLOG_ERROR("Invalid texture handle in render target");
        return { 0, 0 };
    }
    else {
        SPDLOG_ERROR("Invalid render target configuration");
        return { 0, 0 };
    }
}

std::vector<AttachmentHandle> RenderGraphExecutor::resolveNodeAttachments(
    const RenderNode& renderNode,
    uint32_t swapchainImageIndex) {

    std::vector<AttachmentHandle> handles;
    const auto& swapchainAttachments = m_swapChain.getAttachmentHandles();

    // Color attachments
    for (const auto& attachment : renderNode.getColorAttachments()) {
        if (attachment.source == AttachmentSource::Swapchain) {
            handles.push_back(swapchainAttachments[swapchainImageIndex]);
        }
        else {
            handles.push_back(attachment.handle);
        }
    }

    // Depth attachment
    if (const auto* depthAttachment = renderNode.getDepthAttachment()) {
        handles.push_back(depthAttachment->handle);
    }

    // Resolve attachment
    if (const auto* resolveAttachment = renderNode.getResolveAttachment()) {
        if (resolveAttachment->source == AttachmentSource::Swapchain) {
            handles.push_back(swapchainAttachments[swapchainImageIndex]);
        }
        else {
            handles.push_back(resolveAttachment->handle);
        }
    }

    return handles;
}