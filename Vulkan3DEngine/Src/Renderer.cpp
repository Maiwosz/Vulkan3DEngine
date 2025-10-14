#include "Renderer.h"
#include "MeshRenderer.h"
#include "Pipeline.h"
#include "GpuCall.h"
#include <stdexcept>
#include <array>
#include <spdlog/spdlog.h>

Renderer::Renderer(
    EngineCore& engineCore,
    VulkanContext& vulkanContext,
    FrameManager& frameManager,
    VramManager& vramManager,
    SwapChain& swapChain,
    AttachmentManager& attachmentManager,
    FrameBufferManager& framebufferManager,
    RenderPassManager& renderPassManager,
    DescriptorAllocator& descriptorAllocator,
    PipelineManager& pipelineManager)
    : m_engineCore(engineCore),
    m_vulkanContext(vulkanContext),
    m_frameManager(frameManager),
    m_vramManager(vramManager),
    m_swapChain(swapChain),
    m_attachmentManager(attachmentManager),
    m_framebufferManager(framebufferManager),
    m_renderPassManager(renderPassManager),
    m_descriptorAllocator(descriptorAllocator),
    m_pipelineManager(pipelineManager)
{

    // Create service objects - simplified initialization
    m_meshRenderer = std::make_unique<MeshRenderer>(m_vulkanContext, m_vramManager);

    m_renderGraphExecutor = std::make_unique<RenderGraphExecutor>(
        engineCore,
        *this
    );
}

Renderer::~Renderer() {
    if (m_frameActive) {
        cleanupFrame();
    }
}

void Renderer::assignRenderGraph(const SmartRenderGraphHandle& renderGraphHandle) {
    if (!m_renderGraphExecutor) {
        SPDLOG_ERROR("Renderer: RenderGraphExecutor not initialized");
        return;
    }

    m_renderGraphExecutor->assignRenderGraph(renderGraphHandle);

    if (renderGraphHandle.isValid()) {
        SPDLOG_DEBUG("Renderer: Assigned render graph (ID: {})", renderGraphHandle.handle().id);
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

const SmartRenderGraphHandle& Renderer::getAssignedRenderGraphHandle() const {
    if (!m_renderGraphExecutor) {
        static SmartRenderGraphHandle invalidHandle;
        return invalidHandle;
    }
    return m_renderGraphExecutor->getAssignedGraphHandle();
}

bool Renderer::executeRenderGraph(const std::vector<std::unique_ptr<GpuCall>>& gpuCalls) {
    if (!m_frameActive) {
        SPDLOG_ERROR("Renderer: Cannot execute render graph - no active frame");
        return false;
    }

    if (!m_renderGraphExecutor) {
        SPDLOG_ERROR("Renderer: RenderGraphExecutor not initialized");
        return false;
    }

    if (!hasAssignedRenderGraph()) {
        SPDLOG_ERROR("Renderer: No render graph assigned");
        return false;
    }

    try {
        bool success = m_renderGraphExecutor->executeGpuCalls(gpuCalls);

        if (success) {
            SPDLOG_DEBUG("Renderer: Successfully executed render graph with {} GPU calls",
                gpuCalls.size());
        }
        else {
            SPDLOG_ERROR("Renderer: Failed to execute render graph");
        }

        return success;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Renderer: Exception during render graph execution: {}", e.what());
        return false;
    }
}

bool Renderer::beginFrame() {
    if (m_frameActive) {
        return false;
    }

    try {
        prepareFrame();
        m_currentImageIndex = acquireSwapchainImage();

        m_frameActive = true;

        return true;
    }
    catch (const std::exception&) {
        cleanupFrame();
        return false;
    }
}

void Renderer::endFrame() {
    if (!m_frameActive) {
        return;
    }

    try {
        submitAndPresent();
    }
    catch (const std::exception&) {
        // Frame will be cleaned up regardless
    }

    cleanupFrame();
    m_engineCore.advanceFrame();
}

bool Renderer::bindPipeline(PipelineHandle pipelineHandle) {
    ensureFrameActive();

    if (!pipelineHandle.isValid() || !m_pipelineManager.isValid(pipelineHandle)) {
        SPDLOG_ERROR("Invalid pipeline handle: {}", pipelineHandle.id);
        return false;
    }

    try {
        Pipeline& pipeline = m_pipelineManager.get(pipelineHandle);
        VkPipeline vkPipeline = pipeline.get();
        VkPipelineLayout layout = pipeline.getLayout();

        if (vkPipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE) {
            SPDLOG_ERROR("Pipeline or layout is null");
            return false;
        }

        vkCmdBindPipeline(getCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);

        m_currentPipeline = pipelineHandle;
        m_currentPipelineLayout = layout;

        SPDLOG_DEBUG("Pipeline bound: {}", pipelineHandle.id);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception binding pipeline: {}", e.what());
        return false;
    }
}

bool Renderer::bindDescriptorSets(const std::vector<DescriptorSetHandle>& descriptorHandles) {
    ensureFrameActive();

    if (m_currentPipelineLayout == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Cannot bind descriptors - no pipeline bound");
        return false;
    }

    if (descriptorHandles.empty()) {
        SPDLOG_DEBUG("No descriptor sets to bind");
        return true;
    }

    // Konwertuj uchwyty na VkDescriptorSet i waliduj
    std::vector<VkDescriptorSet> descriptorSets;
    descriptorSets.reserve(descriptorHandles.size());

    for (size_t i = 0; i < descriptorHandles.size(); ++i) {
        const auto& handle = descriptorHandles[i];

        if (!handle.isValid()) {
            SPDLOG_ERROR("Invalid descriptor set handle at index {}", i);
            return false;
        }

        VkDescriptorSet descriptorSet = m_descriptorAllocator.getDescriptorSet(handle);
        if (descriptorSet == VK_NULL_HANDLE) {
            SPDLOG_ERROR("Null descriptor set at index {} (handle: {})", i, handle.id);
            return false;
        }

        descriptorSets.push_back(descriptorSet);
    }

    // KLUCZOWE: Oznacz wszystkie deskryptory jako używane przez GPU PRZED bindowaniem
    uint32_t currentFrameIndex = m_frameManager.getCurrentFrameIndex();
    for (const auto& handle : descriptorHandles) {
        m_descriptorAllocator.markDescriptorAsUsedByGPU(handle, currentFrameIndex);
    }

    // Dopiero teraz zbinduj deskryptory do command buffera
    vkCmdBindDescriptorSets(
        getCurrentCommandBuffer(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_currentPipelineLayout,
        0,
        static_cast<uint32_t>(descriptorSets.size()),
        descriptorSets.data(),
        0,
        nullptr
    );

    SPDLOG_DEBUG("Descriptor sets bound: {} (frame: {})", descriptorSets.size(), currentFrameIndex);
    return true;
}

// FIXED IMPLEMENTATION - Command buffer access methods
VkCommandBuffer Renderer::getCurrentCommandBuffer() const {
    ensureFrameActive();
    return m_frameManager.getCurrentFrame().graphicsCommandBuffer->handle();
}

VkCommandBuffer Renderer::getTransferCommandBuffer() const {
    ensureFrameActive();
    return m_frameManager.getCurrentFrame().transferCommandBuffer->handle();
}

VkCommandBuffer Renderer::getImGuiCommandBuffer() const {
    ensureFrameActive();
    return m_frameManager.getCurrentFrame().imguiCommandBuffer->handle();
}

void Renderer::prepareFrame() {
    auto inFlightFence = m_frameManager.getInFlightFence();
    vkWaitForFences(m_vulkanContext.logical().get(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_vulkanContext.logical().get(), 1, &inFlightFence);
    m_descriptorAllocator.markFrameCompleted(m_frameManager.getCurrentFrameIndex());

    auto& currentFrame = m_frameManager.getCurrentFrame();

    auto resetAndBegin = [](auto& cmdBuffer) {
        if (cmdBuffer->isRecording()) {
            cmdBuffer->end();
        }
        cmdBuffer->reset();
        cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        };

    resetAndBegin(currentFrame.graphicsCommandBuffer);
    resetAndBegin(currentFrame.transferCommandBuffer);

    if (m_vramManager.transferManager().hasPendingTransfers()) {
        m_vramManager.transferManager().executeCompleteTransferPass();
    }
}

uint32_t Renderer::acquireSwapchainImage() {
    uint32_t imageIndex;
    VkResult result = m_swapChain.acquireNextImage(m_frameManager.getImageAvailableSemaphore(), &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        handleSwapchainRecreation();
        throw std::runtime_error("Swapchain recreation needed");
    }

    if (result != VK_SUCCESS) {
        auto& currentFrame = m_frameManager.getCurrentFrame();
        auto cmdBuffer = currentFrame.graphicsCommandBuffer.get();

        if (!cmdBuffer->isRecording()) {
            cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        }
        cmdBuffer->end();
        cmdBuffer->submit(
            m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics),
            {}, {}, {}, m_frameManager.getInFlightFence()
        );

        throw std::runtime_error("Failed to acquire swapchain image");
    }

    return imageIndex;
}

void Renderer::submitAndPresent() {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    if (currentFrame.graphicsCommandBuffer->isRecording()) {
        currentFrame.graphicsCommandBuffer->end();
    }

    std::vector<VkSemaphore> waitSemaphores = { m_frameManager.getImageAvailableSemaphore() };
    std::vector<VkPipelineStageFlags> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    if (m_vramManager.transferManager().hadTransfersThisFrame()) {
        waitSemaphores.push_back(m_frameManager.gettransferFinishedSemaphore());
        waitStages.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
    }

    std::vector<VkSemaphore> signalSemaphores = { m_frameManager.getRenderFinishedSemaphore() };

    currentFrame.graphicsCommandBuffer->submit(
        m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics),
        waitSemaphores, waitStages, signalSemaphores,
        m_frameManager.getInFlightFence()
    );

    VkResult result = m_swapChain.presentImage(m_currentImageIndex, m_frameManager.getRenderFinishedSemaphore());

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        handleSwapchainRecreation();
        throw std::runtime_error("Swapchain recreation after present");
    }

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present image");
    }
}

void Renderer::cleanupFrame() {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    if (currentFrame.graphicsCommandBuffer && currentFrame.graphicsCommandBuffer->isRecording()) {
        currentFrame.graphicsCommandBuffer->end();
    }
    if (currentFrame.transferCommandBuffer && currentFrame.transferCommandBuffer->isRecording()) {
        currentFrame.transferCommandBuffer->end();
    }

    m_currentPipeline = PipelineHandle(0);
    m_currentPipelineLayout = VK_NULL_HANDLE;

    m_frameActive = false;
    m_currentImageIndex = 0;
    m_vramManager.transferManager().resetFrameState();
}

void Renderer::setViewport(VkExtent2D extent, float minDepth, float maxDepth) {
    setViewport(0, 0, extent.width, extent.height, minDepth, maxDepth);
}

void Renderer::setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height, float minDepth, float maxDepth) {
    ensureFrameActive();

    VkViewport viewport{};
    viewport.x = static_cast<float>(x);
    viewport.y = static_cast<float>(y);
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;

    vkCmdSetViewport(getCurrentCommandBuffer(), 0, 1, &viewport);
}

void Renderer::setScissor(VkExtent2D extent, VkOffset2D offset) {
    setScissor(offset.x, offset.y, extent.width, extent.height);
}

void Renderer::setScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    ensureFrameActive();

    VkRect2D scissor{};
    scissor.offset = { static_cast<int32_t>(x), static_cast<int32_t>(y) };
    scissor.extent = { width, height };

    vkCmdSetScissor(getCurrentCommandBuffer(), 0, 1, &scissor);
}

void Renderer::handleSwapchainRecreation() {
    cleanupFrame();

    vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics));
    vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Transfer));

    m_engineCore.recreateSwapChain();
}

void Renderer::ensureFrameActive() const {
    if (!m_frameActive) {
        throw std::runtime_error("No active frame");
    }
}