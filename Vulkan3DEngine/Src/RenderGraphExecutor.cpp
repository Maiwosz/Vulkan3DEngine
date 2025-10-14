#include "RenderGraphExecutor.h"
#include "Renderer.h"
#include "GpuCall.h"
#include "RenderNode.h"
#include "RenderGraph.h"
#include "RenderTarget.h"
#include "FrameBufferManager.h"
#include "AttachmentManager.h"
#include "SwapChain.h"
#include "IImGuiProvider.h"
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

void RenderGraphExecutor::assignRenderGraph(const SmartRenderGraphHandle& renderGraphHandle) {
    m_assignedGraphHandle = renderGraphHandle;

    // Mark ImGui as needing reinitialization when graph changes
    if (m_imguiProvider) {
        m_imguiNeedsInit = true;
    }

    if (m_assignedGraphHandle) {
        SPDLOG_DEBUG("RenderGraphExecutor: Assigned render graph with {} nodes",
            m_assignedGraphHandle->getNodeCount());
    }
    else {
        SPDLOG_DEBUG("RenderGraphExecutor: Cleared render graph assignment");
    }
}

void RenderGraphExecutor::attachImGuiProvider(std::shared_ptr<IImGuiProvider> provider) {
    m_imguiProvider = provider;
    m_imguiNeedsInit = true;

    if (m_imguiProvider) {
        SPDLOG_DEBUG("RenderGraphExecutor: Attached ImGui provider");
    }
    else {
        SPDLOG_DEBUG("RenderGraphExecutor: Detached ImGui provider");
    }
}

bool RenderGraphExecutor::executeGpuCalls(const std::vector<std::unique_ptr<GpuCall>>& gpuCalls) {
    if (!m_assignedGraphHandle) {
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
        const auto& nodes = m_assignedGraphHandle->getNodes();
        const size_t finalNodeIndex = nodes.size() - 1;

        // Initialize ImGui provider early if we have one (before beginFrame)
        if (m_imguiProvider && !nodes.empty()) {
            const auto& finalNode = nodes[finalNodeIndex];
            if (finalNode && finalNode->isValid()) {
                ensureImGuiProviderReady(*finalNode);
            }
        }

        // Begin ImGui frame if provider attached and initialized
        if (m_imguiProvider && m_imguiProvider->isInitialized()) {
            m_imguiProvider->beginFrame();
        }

        for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
            const auto& renderNode = nodes[nodeIndex];

            if (!renderNode) {
                SPDLOG_ERROR("Null render node at index {}", nodeIndex);
                return false;
            }

            if (!renderNode->isValid()) {
                SPDLOG_ERROR("Invalid render node at index {}", nodeIndex);
                return false;
            }

            bool isFinalNode = (nodeIndex == finalNodeIndex);

            if (!executeNodeGpuCalls(nodeIndex, *renderNode, gpuCalls, isFinalNode)) {
                SPDLOG_ERROR("Failed to execute GPU calls on node {}", nodeIndex);
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

bool RenderGraphExecutor::executeNodeGpuCalls(
    size_t nodeIndex,
    RenderNode& renderNode,
    const std::vector<std::unique_ptr<GpuCall>>& gpuCalls,
    bool isFinalNode) {

    // Setup framebuffer for this node
    FrameBufferHandle framebufferHandle = setupNodeFramebuffer(nodeIndex, renderNode);
    if (!framebufferHandle.isValid()) {
        SPDLOG_ERROR("Failed to setup framebuffer for render node {}", nodeIndex);
        return false;
    }

    // Begin render pass for this node
    if (!beginNodeRenderPass(nodeIndex, renderNode, framebufferHandle)) {
        SPDLOG_ERROR("Failed to begin render pass for node {}", nodeIndex);
        return false;
    }

    // Execute all GPU calls within this render pass
    bool success = true;
    for (const auto& gpuCall : gpuCalls) {
        if (!gpuCall) {
            SPDLOG_ERROR("Null GPU call encountered");
            success = false;
            break;
        }

        // Execute the GPU call with render node context
        if (!gpuCall->execute(m_renderer, m_engineCore, renderNode)) {
            SPDLOG_ERROR("Failed to execute GPU call");
            success = false;
            break;
        }
    }

    // If this is the final node and ImGui provider is attached, render ImGui
    if (success && isFinalNode && m_imguiProvider && m_imguiProvider->isInitialized()) {
        renderImGui();
    }

    // End render pass for this node
    endNodeRenderPass();

    return success;
}

FrameBufferHandle RenderGraphExecutor::setupNodeFramebuffer(
    size_t nodeIndex,
    RenderNode& renderNode) {

    // Determine current swapchain image index if needed
    uint32_t swapchainImageIndex = 0;
    if (m_assignedGraphHandle->getRenderTarget().isSwapchain()) {
        swapchainImageIndex = m_renderer.getCurrentImageIndex();
    }

    // Resolve attachments for this node from the graph
    std::vector<BoundAttachment> boundAttachments =
        m_assignedGraphHandle->resolveNodeAttachments(
            static_cast<uint32_t>(nodeIndex),
            swapchainImageIndex
        );

    if (boundAttachments.empty()) {
        SPDLOG_ERROR("Node {} has no attachments", nodeIndex);
        return FrameBufferHandle(0);
    }

    // Extract attachment handles in binding order
    std::vector<AttachmentHandle> attachmentHandles;
    attachmentHandles.reserve(boundAttachments.size());

    for (const auto& bound : boundAttachments) {
        attachmentHandles.push_back(bound.handle);
    }

    // Get extent from the graph
    VkExtent2D extent = m_assignedGraphHandle->getExtent();

    if (extent.width == 0 || extent.height == 0) {
        SPDLOG_ERROR("Invalid extent for node {}: {}x{}",
            nodeIndex, extent.width, extent.height);
        return FrameBufferHandle(0);
    }

    // Create framebuffer configuration
    FrameBufferConfig config;
    config.renderPassHandle = renderNode.getSmartRenderPassHandle().handle();
    config.attachmentHandles = attachmentHandles;
    config.extent = extent;

    // Acquire framebuffer from manager (cached automatically)
    FrameBufferHandle handle = m_framebufferManager.acquireFrameBuffer(config);

    if (!handle.isValid()) {
        SPDLOG_ERROR("Failed to acquire framebuffer for node {}", nodeIndex);
    }

    return handle;
}

bool RenderGraphExecutor::beginNodeRenderPass(
    size_t nodeIndex,
    RenderNode& renderNode,
    FrameBufferHandle framebufferHandle) {

    if (m_nodeRenderPassActive) {
        SPDLOG_ERROR("Node render pass already active");
        return false;
    }

    // Store current framebuffer
    m_currentNodeFramebuffer = framebufferHandle;

    // Get framebuffer resource
    auto* framebuffer = m_framebufferManager.getResource(framebufferHandle);
    if (!framebuffer) {
        SPDLOG_ERROR("Invalid framebuffer handle for node {}", nodeIndex);
        return false;
    }

    // Get clear values from graph
    std::vector<VkClearValue> clearValues =
        m_assignedGraphHandle->getNodeClearValues(static_cast<uint32_t>(nodeIndex));

    // Setup render pass begin info
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderNode.getRenderPass();
    renderPassInfo.framebuffer = framebuffer->frameBuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = framebuffer->config.extent;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // Begin render pass
    vkCmdBeginRenderPass(
        m_renderer.getCurrentCommandBuffer(),
        &renderPassInfo,
        VK_SUBPASS_CONTENTS_INLINE
    );

    m_nodeRenderPassActive = true;

    // Set viewport and scissor for this framebuffer
    m_renderer.setViewport(framebuffer->config.extent);
    m_renderer.setScissor(framebuffer->config.extent);

    SPDLOG_DEBUG("Began render pass for node {}, extent: {}x{}",
        nodeIndex,
        framebuffer->config.extent.width,
        framebuffer->config.extent.height);

    return true;
}

void RenderGraphExecutor::endNodeRenderPass() {
    if (!m_nodeRenderPassActive) {
        return;
    }

    vkCmdEndRenderPass(m_renderer.getCurrentCommandBuffer());

    m_nodeRenderPassActive = false;
    m_currentNodeFramebuffer = FrameBufferHandle(0);

    SPDLOG_DEBUG("Ended render pass for node");
}

bool RenderGraphExecutor::ensureImGuiProviderReady(RenderNode& finalNode) {
    if (!m_imguiProvider) {
        return false;
    }

    // Check if initialization is needed
    if (m_imguiNeedsInit || !m_imguiProvider->isInitialized()) {
        // Get render pass and MSAA samples from the final node
        SmartRenderPassHandle renderPassHandle = finalNode.getSmartRenderPassHandle();
        VkSampleCountFlagBits msaaSamples = m_assignedGraphHandle->getTargetSampleCount();

        SPDLOG_DEBUG("Initializing ImGui provider for final render pass (MSAA: {}x)",
            static_cast<uint32_t>(msaaSamples));

        if (!m_imguiProvider->initialize(renderPassHandle, msaaSamples)) {
            SPDLOG_ERROR("Failed to initialize ImGui provider");
            return false;
        }

        m_imguiNeedsInit = false;
        SPDLOG_DEBUG("ImGui provider initialized successfully");
    }

    return true;
}

void RenderGraphExecutor::renderImGui() {
    if (!m_imguiProvider || !m_imguiProvider->isInitialized()) {
        return;
    }

    // Execute all registered UI callbacks
    m_imguiProvider->executeCallbacks();

    // End ImGui frame (prepares draw data)
    m_imguiProvider->endFrame();

    // Render to command buffer
    m_imguiProvider->render(m_renderer.getCurrentCommandBuffer());

    SPDLOG_DEBUG("Rendered ImGui UI ({} callbacks)",
        m_imguiProvider->getCallbackCount());
}