#include "RenderStage.h"
#include <array>
#include <stdexcept>
#include "Engine.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "MeshRenderOrder.h"

RenderStage::RenderStage(Renderer& renderer, AssetSystem& assetSystem)
    : m_renderer(renderer),
    m_vulkanContext(renderer.vulkanContext()),
    m_frameManager(renderer.frameManager()),
    m_vramManager(renderer.vramManager()),
    m_swapChain(renderer.swapChain()),
    m_attachmentManager(renderer.attachmentManager()),
    m_framebufferManager(renderer.framebufferManager()),
    m_renderPassManager(renderer.renderPassManager()),
    m_pipelineManager(renderer.pipelineManager()),
    m_assetSystem(assetSystem),
    m_depthAttachmentHandle(renderer.depthAttachmentHandle()),
    m_msColorAttachmentHandle(renderer.msColorAttachmentHandle()),
    m_mainRenderPassHandle(renderer.renderPass()),
    m_descriptorAllocator(renderer.descriptorAllocator()) {
    SPDLOG_INFO("RenderStage initialized");
}

void RenderStage::process(std::shared_ptr<RenderOrder> order) {
    m_frameManager.getCurrentFrame().renderOrders.push_back(order);
    SPDLOG_DEBUG("RenderOrder added: type={}", static_cast<int>(order->getType()));
}

void RenderStage::executeRenderPass() {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    SPDLOG_INFO("Beginning render pass execution (frame {})", m_frameManager.getCurrentFrameIndex());
    SPDLOG_INFO("RenderOrders count: {}", currentFrame.renderOrders.size());

    if (currentFrame.renderOrders.empty()) {
        SPDLOG_WARN("No render orders to process in current frame!");
    }

    try {
        waitForPreviousFrame();
        releaseCurrentFramebuffer();
        resetAndBeginCommandBuffers();
        executeTransferOperations();

        uint32_t imageIndex = acquireSwapchainImage();

        // Validate swapchain image index
        if (imageIndex >= m_swapChain.getImages().size()) {
            SPDLOG_ERROR("Invalid swapchain image index: {}, max: {}",
                imageIndex, m_swapChain.getImages().size());
            throw std::runtime_error("Invalid swapchain image index");
        }

        FrameBufferHandle framebufferHandle = createFramebufferForImage(imageIndex);

        // Validate framebuffer handle
        if (!framebufferHandle.isValid() || !m_framebufferManager.isValid(framebufferHandle)) {
            SPDLOG_ERROR("Failed to create valid framebuffer for image index: {}", imageIndex);
            throw std::runtime_error("Invalid framebuffer handle");
        }

        // Validate framebuffer dimensions
        auto* framebufferResource = m_framebufferManager.getResource(framebufferHandle);
        if (!framebufferResource) {
            SPDLOG_ERROR("Framebuffer resource is null for handle: {}", framebufferHandle.id);
            throw std::runtime_error("Null framebuffer resource");
        }

        VkExtent2D swapchainExtent = m_swapChain.getSwapChainExtent();
        if (framebufferResource->config.extent.width != swapchainExtent.width ||
            framebufferResource->config.extent.height != swapchainExtent.height) {
            SPDLOG_ERROR("Framebuffer dimensions mismatch: {}x{} vs {}x{}",
                framebufferResource->config.extent.width,
                framebufferResource->config.extent.height,
                swapchainExtent.width,
                swapchainExtent.height);
            throw std::runtime_error("Framebuffer dimension mismatch");
        }

        beginRenderPass(framebufferHandle);
        executeRenderCommands(currentFrame.graphicsCommandBuffer.get()->handle(), imageIndex, currentFrame.renderOrders);
        endRenderPass();

        submitGraphicsCommands();
        presentImage(imageIndex);

        cleanupFrame();
        m_renderer.advanceFrame();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error in render pass execution: {}", e.what());
        endCommandBuffersOnError();
        cleanupFrame();
        m_renderer.advanceFrame();
    }
}

// Frame synchronization helpers
void RenderStage::waitForPreviousFrame() {
    auto inFlightFence = m_frameManager.getInFlightFence();
    SPDLOG_DEBUG("Waiting for in-flight fence: {:#x}", reinterpret_cast<uint64_t>(inFlightFence));

    vkWaitForFences(m_vulkanContext.logical().get(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_vulkanContext.logical().get(), 1, &inFlightFence);
	m_descriptorAllocator.markFrameCompleted(m_frameManager.getCurrentFrameIndex());
    SPDLOG_DEBUG("Fence reset");
}

void RenderStage::signalFenceToPreventDeadlock(VkFence fence) {
    VkResult fenceStatus = vkGetFenceStatus(m_vulkanContext.logical().get(), fence);
    if (fenceStatus == VK_NOT_READY) {
        auto& currentFrame = m_frameManager.getCurrentFrame();
        auto tempCmdBuffer = currentFrame.graphicsCommandBuffer.get();

        if (!tempCmdBuffer->isRecording()) {
            tempCmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        }
        tempCmdBuffer->end();

        tempCmdBuffer->submit(
            m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics),
            {}, {}, {}, fence
        );
        SPDLOG_DEBUG("Empty command buffer submitted to signal fence");
    }
}

// Command buffer management
void RenderStage::resetAndBeginCommandBuffers() {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    // Reset graphics command buffer
    if (currentFrame.graphicsCommandBuffer) {
        if (currentFrame.graphicsCommandBuffer->isRecording()) {
            currentFrame.graphicsCommandBuffer->end();
        }
        currentFrame.graphicsCommandBuffer->reset();
        currentFrame.graphicsCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        SPDLOG_DEBUG("Graphics command buffer reset and began");
    }
    else {
        SPDLOG_ERROR("Graphics command buffer is null!");
    }

    // Reset transfer command buffer
    if (currentFrame.transferCommandBuffer) {
        if (currentFrame.transferCommandBuffer->isRecording()) {
            currentFrame.transferCommandBuffer->end();
        }
        currentFrame.transferCommandBuffer->reset();
        currentFrame.transferCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        SPDLOG_DEBUG("Transfer command buffer reset and began");
    }
    else {
        SPDLOG_ERROR("Transfer command buffer is null!");
    }
}

void RenderStage::endCommandBuffersOnError() {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    if (currentFrame.graphicsCommandBuffer && currentFrame.graphicsCommandBuffer->isRecording()) {
        currentFrame.graphicsCommandBuffer->end();
    }
    if (currentFrame.transferCommandBuffer && currentFrame.transferCommandBuffer->isRecording()) {
        currentFrame.transferCommandBuffer->end();
    }
}

// Transfer operations
void RenderStage::executeTransferOperations() {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    if (m_vramManager.transferManager().hasPendingTransfers()) {
        SPDLOG_INFO("Executing pending VRAM transfers");
        m_vramManager.transferManager().executeTransfers(
            *currentFrame.transferCommandBuffer,
            *currentFrame.graphicsCommandBuffer
        );
        currentFrame.hasTransferCommands = true;
    }

    if (currentFrame.hasTransferCommands) {
        SPDLOG_DEBUG("Submitting transfer commands");
        if (currentFrame.transferCommandBuffer->isRecording()) {
            currentFrame.transferCommandBuffer->end();
        }

        VkSemaphore signalSemaphore = currentFrame.transferFinished;
        currentFrame.transferCommandBuffer->submit(
            m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Transfer),
            {}, {},
            std::span<const VkSemaphore>(&signalSemaphore, 1),
            VK_NULL_HANDLE
        );
    }

    m_vramManager.reclaimStagingBuffers();
}

// Swapchain operations
uint32_t RenderStage::acquireSwapchainImage() {
    uint32_t imageIndex;
    SPDLOG_DEBUG("Acquiring next swapchain image");

    VkResult result = m_swapChain.acquireNextImage(m_frameManager.getImageAvailableSemaphore(), &imageIndex);

    // Fix: Store values in variables to avoid reference issues
    int resultValue = static_cast<int>(result);
    SPDLOG_INFO("Acquired swapchain image: index={}, result={}", imageIndex, resultValue);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        SPDLOG_WARN("Swapchain out of date or suboptimal, recreating");
        handleSwapchainRecreation();
        throw std::runtime_error("Swapchain recreation needed");
    }
    else if (result != VK_SUCCESS) {
        SPDLOG_ERROR("Failed to acquire swap chain image! Result: {}", resultValue);
        signalFenceToPreventDeadlock(m_frameManager.getInFlightFence());
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    return imageIndex;
}

void RenderStage::handleSwapchainRecreation() {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    endCommandBuffersOnError();
    cleanupFrame();
    signalFenceToPreventDeadlock(m_frameManager.getInFlightFence());

    vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics));
    vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Transfer));

    m_renderer.recreateSwapChain();
    m_depthAttachmentHandle = m_renderer.depthAttachmentHandle();
    m_msColorAttachmentHandle = m_renderer.msColorAttachmentHandle();
    m_mainRenderPassHandle = m_renderer.renderPass();
}

void RenderStage::presentImage(uint32_t imageIndex) {
    SPDLOG_DEBUG("Presenting image index: {}", imageIndex);
    VkResult result = m_swapChain.presentImage(imageIndex, m_frameManager.getRenderFinishedSemaphore());

    // Fix: Store value in variable to avoid reference issues
    int resultValue = static_cast<int>(result);
    SPDLOG_INFO("Present result: {}", resultValue);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        SPDLOG_WARN("Swapchain out of date or suboptimal after present, recreating");
        cleanupFrame();

        vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics));
        vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Transfer));

        m_renderer.recreateSwapChain();
        m_depthAttachmentHandle = m_renderer.depthAttachmentHandle();
        m_msColorAttachmentHandle = m_renderer.msColorAttachmentHandle();
        m_mainRenderPassHandle = m_renderer.renderPass();

        throw std::runtime_error("Swapchain recreation after present");
    }
    else if (result != VK_SUCCESS) {
        SPDLOG_ERROR("Failed to present swap chain image! Result: {}", resultValue);
        throw std::runtime_error("Failed to present swap chain image!");
    }
}

// Framebuffer management
void RenderStage::releaseCurrentFramebuffer() {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    if (currentFrame.framebufferHandle.id != 0) {
        // Fix: Store ID in variable to avoid reference issues
        uint32_t handleId = currentFrame.framebufferHandle.id;
        SPDLOG_DEBUG("Releasing previous frame's framebuffer handle: {}", handleId);
        m_framebufferManager.removeReference(currentFrame.framebufferHandle);
        currentFrame.framebufferHandle = FrameBufferHandle(0);
    }
}

FrameBufferHandle RenderStage::createFramebufferForImage(uint32_t imageIndex) {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    // Validate image index
    const auto& imageHandles = m_swapChain.getImageHandles();
    if (imageIndex >= imageHandles.size()) {
        SPDLOG_ERROR("Image index {} out of range, max: {}", imageIndex, imageHandles.size());
        return FrameBufferHandle(0);
    }

    VramHandle swapchainImageHandle = imageHandles[imageIndex];
    if (!swapchainImageHandle.isValid()) {
        SPDLOG_ERROR("Invalid swapchain image handle for index: {}", imageIndex);
        return FrameBufferHandle(0);
    }

    uint64_t handleId = swapchainImageHandle.id;
    SPDLOG_DEBUG("Using swapchain image handle: {}", handleId);

    // Validate attachment handles
    if (!m_attachmentManager.isValid(m_msColorAttachmentHandle)) {
        SPDLOG_ERROR("Invalid multisampling color attachment handle: {}", m_msColorAttachmentHandle.id);
        return FrameBufferHandle(0);
    }

    if (!m_attachmentManager.isValid(m_depthAttachmentHandle)) {
        SPDLOG_ERROR("Invalid depth attachment handle: {}", m_depthAttachmentHandle.id);
        return FrameBufferHandle(0);
    }

    // Validate render pass handle
    if (!m_renderPassManager.isValid(m_mainRenderPassHandle)) {
        SPDLOG_ERROR("Invalid render pass handle: {}", m_mainRenderPassHandle.id);
        return FrameBufferHandle(0);
    }

    try {
        AttachmentHandle swapchainAttachmentHandle = m_attachmentManager.registerExternalImage(
            swapchainImageHandle,
            m_swapChain.getImageFormat(),
            m_swapChain.getSwapChainExtent(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            AttachmentType::Resolve
        );

        if (!swapchainAttachmentHandle.isValid()) {
            SPDLOG_ERROR("Failed to register swapchain image as attachment");
            return FrameBufferHandle(0);
        }

        uint32_t attachmentHandleId = swapchainAttachmentHandle.id;
        SPDLOG_DEBUG("Created swapchain attachment handle: {}", attachmentHandleId);

        std::vector<AttachmentHandle> framebufferAttachments = {
            m_msColorAttachmentHandle,
            swapchainAttachmentHandle,
            m_depthAttachmentHandle
        };

        FrameBufferHandle framebufferHandle = m_framebufferManager.acquireFrameBuffer(
            m_mainRenderPassHandle,
            framebufferAttachments,
            m_swapChain.getSwapChainExtent()
        );

        if (!framebufferHandle.isValid()) {
            SPDLOG_ERROR("Failed to acquire framebuffer");
            return FrameBufferHandle(0);
        }

        currentFrame.framebufferHandle = framebufferHandle;
        uint32_t framebufferHandleId = framebufferHandle.id;
        SPDLOG_DEBUG("Using framebuffer handle: {}", framebufferHandleId);

        return framebufferHandle;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in createFramebufferForImage: {}", e.what());
        return FrameBufferHandle(0);
    }
}

// Render pass execution
void RenderStage::beginRenderPass(FrameBufferHandle framebufferHandle) {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    // Validate framebuffer handle
    if (!framebufferHandle.isValid() || !m_framebufferManager.isValid(framebufferHandle)) {
        SPDLOG_ERROR("Invalid framebuffer handle: {}", framebufferHandle.id);
        throw std::runtime_error("Invalid framebuffer handle in beginRenderPass");
    }

    // Validate render pass handle
    if (!m_renderPassManager.isValid(m_mainRenderPassHandle)) {
        SPDLOG_ERROR("Invalid render pass handle: {}", m_mainRenderPassHandle.id);
        throw std::runtime_error("Invalid render pass handle in beginRenderPass");
    }

    // Get resources
    auto* renderPass = m_renderPassManager.getResource(m_mainRenderPassHandle);
    if (!renderPass || *renderPass == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Render pass resource is null or invalid");
        throw std::runtime_error("Invalid render pass resource");
    }

    auto* framebufferResource = m_framebufferManager.getResource(framebufferHandle);
    if (!framebufferResource || framebufferResource->frameBuffer == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Framebuffer resource is null or invalid");
        throw std::runtime_error("Invalid framebuffer resource");
    }

    // Validate dimensions match
    VkExtent2D swapchainExtent = m_swapChain.getSwapChainExtent();
    if (framebufferResource->config.extent.width != swapchainExtent.width ||
        framebufferResource->config.extent.height != swapchainExtent.height) {
        SPDLOG_ERROR("Framebuffer extent mismatch with swapchain: {}x{} vs {}x{}",
            framebufferResource->config.extent.width,
            framebufferResource->config.extent.height,
            swapchainExtent.width,
            swapchainExtent.height);
        throw std::runtime_error("Framebuffer extent mismatch");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = *renderPass;
    renderPassInfo.framebuffer = framebufferResource->frameBuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapchainExtent;

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
    clearValues[1].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[2].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    uint32_t width = swapchainExtent.width;
    uint32_t height = swapchainExtent.height;
    SPDLOG_INFO("Beginning render pass with dimensions: {}x{}", width, height);

    vkCmdBeginRenderPass(
        currentFrame.graphicsCommandBuffer.get()->handle(),
        &renderPassInfo,
        VK_SUBPASS_CONTENTS_INLINE
    );
}

void RenderStage::endRenderPass() {
    auto& currentFrame = m_frameManager.getCurrentFrame();
    vkCmdEndRenderPass(currentFrame.graphicsCommandBuffer.get()->handle());
    SPDLOG_DEBUG("Render pass ended");
}

// Command submission
void RenderStage::submitGraphicsCommands() {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    if (currentFrame.graphicsCommandBuffer->isRecording()) {
        currentFrame.graphicsCommandBuffer->end();
    }

    std::vector<VkSemaphore> waitSemaphores = {
        m_frameManager.getImageAvailableSemaphore()
    };
    std::vector<VkPipelineStageFlags> waitStages = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    if (currentFrame.hasTransferCommands) {
        waitSemaphores.push_back(m_frameManager.gettransferFinishedSemaphore());
        waitStages.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
        SPDLOG_DEBUG("Added transfer semaphore to wait list");
    }

    std::vector<VkSemaphore> signalSemaphores = { m_frameManager.getRenderFinishedSemaphore() };

    try {
        SPDLOG_DEBUG("Submitting graphics command buffer");
        currentFrame.graphicsCommandBuffer->submit(
            m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics),
            waitSemaphores,
            waitStages,
            signalSemaphores,
            m_frameManager.getInFlightFence()
        );
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error submitting graphics command buffer: {}", e.what());
        throw;
    }
}

// Cleanup
void RenderStage::cleanupFrame() {
    auto& currentFrame = m_frameManager.getCurrentFrame();
    currentFrame.renderOrders.clear();
    currentFrame.hasTransferCommands = false;
}

// Render command execution
void RenderStage::executeRenderCommands(
    VkCommandBuffer commandBuffer,
    uint32_t imageIndex,
    const std::vector<std::shared_ptr<RenderOrder>>& renderOrders
) {
    // Fix: Store size in variable to avoid reference issues
    size_t orderCount = renderOrders.size();
    SPDLOG_INFO("Executing render commands for {} orders", orderCount);

    // Renderuj standardowe obiekty 3D
    for (const auto& order : renderOrders) {
        if (order->getType() == RenderOrderType::Mesh) {
            auto meshOrder = static_cast<MeshRenderOrder*>(order.get());
            meshOrder->execute(commandBuffer, m_renderer, m_assetSystem);
        }
        else {
            SPDLOG_WARN("Unsupported render order type: {}", static_cast<int>(order->getType()));
        }
    }

    // Renderuj UI na końcu (po wszystkich obiektach 3D)
    renderUI(commandBuffer);
}

void RenderStage::renderUI(VkCommandBuffer commandBuffer) {
    SPDLOG_DEBUG("Beginning UI rendering");

    // Rozpoczynamy nową ramkę ImGui
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Wywołaj callback jeśli został ustawiony
    if (m_uiRenderCallback) {
        m_uiRenderCallback();
    }

    // Renderuj UI za pomocą ImGuiWrapper
    m_renderer.imguiWrapper().render(commandBuffer);

    SPDLOG_DEBUG("UI rendering completed");
}