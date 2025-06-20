#include "RenderStage.h"
#include <array>
#include <stdexcept>
#include "Engine.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

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
    m_meshManager(assetSystem.meshManager()),
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
        FrameBufferHandle framebufferHandle = createFramebufferForImage(imageIndex);

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

    VramHandle swapchainImageHandle = m_swapChain.getImageHandles()[imageIndex];
    // Fix: Store ID in variable to avoid reference issues
    uint32_t handleId = swapchainImageHandle.id;
    SPDLOG_DEBUG("Using swapchain image handle: {}", handleId);

    AttachmentHandle swapchainAttachmentHandle = m_attachmentManager.registerExternalImage(
        swapchainImageHandle,
        m_swapChain.getImageFormat(),
        m_swapChain.getSwapChainExtent(),
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        AttachmentType::Resolve
    );

    // Fix: Store ID in variable to avoid reference issues
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

    currentFrame.framebufferHandle = framebufferHandle;
    // Fix: Store ID in variable to avoid reference issues
    uint32_t framebufferHandleId = framebufferHandle.id;
    SPDLOG_DEBUG("Using framebuffer handle: {}", framebufferHandleId);

    return framebufferHandle;
}

// Render pass execution
void RenderStage::beginRenderPass(FrameBufferHandle framebufferHandle) {
    auto& currentFrame = m_frameManager.getCurrentFrame();

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = *m_renderPassManager.getResource(m_mainRenderPassHandle);
    renderPassInfo.framebuffer = m_framebufferManager.getResource(framebufferHandle)->frameBuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_swapChain.getSwapChainExtent();

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
    clearValues[1].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[2].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // Fix: Store values in variables to avoid reference issues
    uint32_t width = m_swapChain.getSwapChainExtent().width;
    uint32_t height = m_swapChain.getSwapChainExtent().height;
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
            executeMeshRenderOrder(commandBuffer, meshOrder);
        }
        else {
            SPDLOG_WARN("Unsupported render order type: {}", static_cast<int>(order->getType()));
        }
    }

    // Renderuj UI na końcu (po wszystkich obiektach 3D)
    renderUI(commandBuffer);
}

void RenderStage::executeMeshRenderOrder(VkCommandBuffer commandBuffer, MeshRenderOrder* meshOrder) {
    // Fix: Store IDs in variables to avoid reference issues
    uint32_t pipelineId = meshOrder->pipelineHandle.id;
    uint32_t meshId = meshOrder->meshHandle.id;
    SPDLOG_DEBUG("Processing mesh render order, pipeline handle: {}, mesh handle: {}", pipelineId, meshId);

    Pipeline* pipeline = &m_pipelineManager.get(meshOrder->pipelineHandle);
    if (!pipeline) {
        SPDLOG_ERROR("Failed to get pipeline from handle: {}", pipelineId);
        return;
    }

    bindPipeline(commandBuffer, meshOrder->pipelineHandle);
    setViewportAndScissor(commandBuffer);
    bindDescriptorSets(commandBuffer, pipeline->getLayout(), meshOrder);

    const Mesh* mesh = m_meshManager.getMesh(meshOrder->meshHandle);
    if (mesh) {
        bindVertexAndIndexBuffers(commandBuffer, mesh);
        drawMesh(commandBuffer, mesh);
    }
    else {
        SPDLOG_ERROR("Mesh not found for handle: {}", meshId);
    }
}

void RenderStage::bindPipeline(VkCommandBuffer commandBuffer, PipelineHandle pipelineHandle) {
    Pipeline* pipeline = &m_pipelineManager.get(pipelineHandle);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->get());
    SPDLOG_DEBUG("Pipeline bound");
}

void RenderStage::setViewportAndScissor(VkCommandBuffer commandBuffer) {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChain.getSwapChainExtent().width);
    viewport.height = static_cast<float>(m_swapChain.getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_swapChain.getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Fix: Store values in variables to avoid reference issues
    float width = viewport.width;
    float height = viewport.height;
    SPDLOG_DEBUG("Viewport and scissor set: {}x{}", width, height);
}

void RenderStage::bindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, MeshRenderOrder* meshOrder) {
    if (meshOrder->globalDescriptorSetHandle.isValid()) {
        VkDescriptorSet globalDescSet = m_descriptorAllocator.getDescriptorSet(meshOrder->globalDescriptorSetHandle.handle());
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &globalDescSet, 0, nullptr);
        // Fix: Store ID in variable to avoid reference issues
        uint32_t handleId = meshOrder->globalDescriptorSetHandle.handle().id;
        SPDLOG_DEBUG("Global descriptor set bound, handle ID: {}", handleId);
    }
    else {
        SPDLOG_WARN("Global descriptor set handle is invalid");
    }

    if (meshOrder->objectDescriptorSetHandle.isValid()) {
        VkDescriptorSet objectDescSet = m_descriptorAllocator.getDescriptorSet(meshOrder->objectDescriptorSetHandle.handle());
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &objectDescSet, 0, nullptr);
        // Fix: Store ID in variable to avoid reference issues
        uint32_t handleId = meshOrder->objectDescriptorSetHandle.handle().id;
        SPDLOG_DEBUG("Object descriptor set bound, handle ID: {}", handleId);
    }
    else {
        SPDLOG_WARN("Object descriptor set handle is invalid");
    }

    if (meshOrder->materialDescriptorSetHandle.isValid()) {
        VkDescriptorSet materialDescSet = m_descriptorAllocator.getDescriptorSet(meshOrder->materialDescriptorSetHandle.handle());
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &materialDescSet, 0, nullptr);
        // Fix: Store ID in variable to avoid reference issues
        uint32_t handleId = meshOrder->materialDescriptorSetHandle.handle().id;
        SPDLOG_DEBUG("Material descriptor set bound, handle ID: {}", handleId);
    }
    else {
        SPDLOG_WARN("Material descriptor set handle is invalid");
    }
}

void RenderStage::bindVertexAndIndexBuffers(VkCommandBuffer commandBuffer, const Mesh* mesh) {
    // Fix: Store values in variables to avoid reference issues
    uint32_t vertexCount = mesh->vertexCount;
    uint32_t indexCount = mesh->indexCount;
    SPDLOG_DEBUG("Using mesh: vertices={}, indices={}", vertexCount, indexCount);

    VkBuffer vertexBuffer = m_vramManager.getResource<Buffer>(mesh->vertexBuffer)->get();
    VkBuffer indexBuffer = m_vramManager.getResource<Buffer>(mesh->indexBuffer)->get();

    if (vertexBuffer == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Vertex buffer handle is NULL!");
        return;
    }

    if (indexBuffer == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Index buffer handle is NULL!");
        return;
    }

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    SPDLOG_DEBUG("Vertex buffer bound: {:#x}", reinterpret_cast<uint64_t>(vertexBuffer));

    VkIndexType vkIndexType = (mesh->indexType == 0) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, vkIndexType);
    SPDLOG_DEBUG("Index buffer bound: {:#x}, type: {}",
        reinterpret_cast<uint64_t>(indexBuffer),
        (vkIndexType == VK_INDEX_TYPE_UINT16 ? "UINT16" : "UINT32"));
}

void RenderStage::drawMesh(VkCommandBuffer commandBuffer, const Mesh* mesh) {
    size_t indexSize = mesh->getIndexSize();
    uint32_t indexCount = mesh->indexCount;
    VkDeviceSize expectedSize = indexSize * indexCount;
    VkDeviceSize actualSize = m_vramManager.getResource<Buffer>(mesh->indexBuffer)->getSize();

    SPDLOG_DEBUG("Index buffer: expected size={}, actual size={}, indexSize={}",
        expectedSize, actualSize, indexSize);

    if (expectedSize > actualSize) {
        SPDLOG_ERROR("Index buffer too small: expected {} bytes, got {} bytes", expectedSize, actualSize);
        uint32_t safeCount = static_cast<uint32_t>(actualSize / indexSize);
        vkCmdDrawIndexed(commandBuffer, safeCount, 1, 0, 0, 0);
        SPDLOG_WARN("Drawing with reduced index count: {}", safeCount);
    }
    else {
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        SPDLOG_INFO("Drawing indexed: count={}", indexCount);
    }
}

void RenderStage::renderUI(VkCommandBuffer commandBuffer) {
    SPDLOG_DEBUG("Beginning UI rendering");

    // Rozpoczynamy nową ramkę ImGui
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Renderuj testowy UI
    renderTestUI();

    // Renderuj UI za pomocą ImGuiWrapper
    m_renderer.imguiWrapper().render(commandBuffer);

    SPDLOG_DEBUG("UI rendering completed");
}

void RenderStage::renderTestUI() {
    // Testowe okno ImGui
    ImGui::Begin("Test UI Window");
    ImGui::Text("Hello from RenderStage!");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
        1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    static float testValue = 0.0f;
    ImGui::SliderFloat("Test Slider", &testValue, 0.0f, 1.0f);

    static int counter = 0;
    if (ImGui::Button("Test Button")) {
        counter++;
    }
    ImGui::SameLine();
    ImGui::Text("Counter: %d", counter);

    // Wyświetl informacje o renderowaniu
    auto& currentFrame = m_frameManager.getCurrentFrame();
    size_t orderCount = currentFrame.renderOrders.size();
    ImGui::Text("Render Orders: %zu", orderCount);
    ImGui::Text("Frame Index: %u", m_frameManager.getCurrentFrameIndex());

    ImGui::End();

    // Dodatkowe małe okno z metrykami
    ImGui::Begin("Metrics", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Vulkan Renderer Status");
    ImGui::Separator();

    VkExtent2D extent = m_swapChain.getSwapChainExtent();
    ImGui::Text("Swapchain: %ux%u", extent.width, extent.height);

    ImGui::End();
}