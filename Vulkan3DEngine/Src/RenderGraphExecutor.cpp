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
#include "DrawCall.h"
#include "PipelineManager.h"

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

    // Begin render pass for this node using RenderNode's own method
    beginNodeRenderPass(renderNode, framebufferHandle);

    // Execute all GPU calls within this render pass
    bool success = true;
    for (const auto& gpuCall : gpuCalls) {
        if (!gpuCall) {
            SPDLOG_ERROR("Null GPU call encountered");
            success = false;
            break;
        }

        // Check if this is a DrawCall that needs pipeline management
        DrawCall* drawCall = dynamic_cast<DrawCall*>(gpuCall.get());
        if (drawCall && drawCall->hasPipelineConfig()) {
            // Get or create pipeline with render pass information
            PipelineHandle pipelineHandle = getOrCreatePipeline(*drawCall,
                renderNode.getRenderPassHandle());

            if (!pipelineHandle.isValid()) {
                SPDLOG_ERROR("Failed to get pipeline for DrawCall");
                success = false;
                break;
            }

            // Bind the pipeline before executing the draw call
            if (!m_renderer.bindPipeline(pipelineHandle)) {
                SPDLOG_ERROR("Failed to bind pipeline for DrawCall");
                success = false;
                break;
            }
        }

        // Execute the GPU call - it has access to renderer context
        if (!gpuCall->execute(m_renderer, m_engineCore)) {
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

    // Pobierz attachments z render node
    std::vector<AttachmentHandle> attachmentHandles;

    // Jeśli node używa swapchain, potrzebujemy aktualnego image index
    uint32_t swapchainImageIndex = 0;
    if (renderNode.usesSwapchainAttachment()) {
        // Pobierz aktualny image index z renderer lub engine core
        swapchainImageIndex = m_renderer.getCurrentImageIndex();

        // Pobierz swapchain attachments
        const auto& swapchainAttachments = m_swapChain.getAttachmentHandles();
        if (swapchainImageIndex >= swapchainAttachments.size()) {
            SPDLOG_ERROR("Invalid swapchain image index: {}", swapchainImageIndex);
            return FrameBufferHandle(0);
        }
    }

    // Zbierz wszystkie attachments w odpowiedniej kolejności
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
    // This delegates to the node's own render pass logic which handles clear values properly
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

    // Use RenderNode's render pass management
    renderNode.endRenderPass(m_renderer.getCurrentCommandBuffer());

    m_nodeRenderPassActive = false;
    m_currentNodeFramebuffer = FrameBufferHandle(0);

    SPDLOG_DEBUG("Ended render pass for node");
}

void RenderGraphExecutor::endNodeRenderPass() {
    if (!m_nodeRenderPassActive) {
        return;
    }

    // Fallback version for exception cleanup - direct Vulkan call
    vkCmdEndRenderPass(m_renderer.getCurrentCommandBuffer());

    m_nodeRenderPassActive = false;
    m_currentNodeFramebuffer = FrameBufferHandle(0);

    SPDLOG_DEBUG("Ended render pass for node (fallback)");
}

PipelineHandle RenderGraphExecutor::getOrCreatePipeline(const DrawCall& drawCall,
    RenderPassHandle renderPassHandle,
    uint32_t subpass) {
    if (!drawCall.hasPipelineConfig()) {
        SPDLOG_ERROR("DrawCall does not have pipeline configuration");
        return PipelineHandle(0);
    }

    // Get the base pipeline configuration from DrawCall
    GraphicsPipelineConfig config = drawCall.getPipelineConfig();

    // Get the VkRenderPass from RenderPassHandle
    VkRenderPass* renderPassResource = m_renderPassManager.getResource(renderPassHandle);
    if (!renderPassResource) {
        SPDLOG_ERROR("Invalid render pass handle");
        return PipelineHandle(0);
    }

    // Complete the pipeline configuration with render pass information
    config.renderPass.renderPass = *renderPassResource;
    config.renderPass.subpass = subpass;

    // Create or retrieve cached pipeline from PipelineManager
    PipelineHandle pipelineHandle = m_pipelineManager.createGraphicsPipeline(config);

    if (!pipelineHandle.isValid()) {
        SPDLOG_ERROR("Failed to create graphics pipeline");
        return PipelineHandle(0);
    }

    SPDLOG_DEBUG("Successfully created/retrieved pipeline for DrawCall");
    return pipelineHandle;
}

VkExtent2D RenderGraphExecutor::getRenderTargetExtent(const RenderTarget& renderTarget) {
    if (renderTarget.isSwapchain()) {
        // Get extent from swapchain
        return m_swapChain.getSwapChainExtent();
    }
    else if (renderTarget.isTexture()) {
        const auto& textureHandle = renderTarget.getTextureHandle();
        if (textureHandle.isValid()) {
            auto* textureInfo = textureHandle.get();
            if (textureInfo) {
                // Convert from texture dimensions to VkExtent2D
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
        // Depth zawsze managed
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
