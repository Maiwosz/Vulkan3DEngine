#include "RenderStage.h"
#include <array>
#include <stdexcept>

#include "Engine.h"

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
    // Add the render order to the current frame's collection
    m_frameManager.getCurrentFrame().renderOrders.push_back(order);
    SPDLOG_DEBUG("RenderOrder added: type={}", static_cast<int>(order->getType()));
}

void RenderStage::executeRenderPass() {
    // Get the current frame data
    auto& currentFrame = m_frameManager.getCurrentFrame();

    SPDLOG_INFO("Beginning render pass execution (frame {})", m_frameManager.getCurrentFrameIndex());
    SPDLOG_INFO("RenderOrders count: {}", currentFrame.renderOrders.size());

    if (currentFrame.renderOrders.empty()) {
        SPDLOG_WARN("No render orders to process in current frame!");
    }

    // Wait for previous frame to finish - IMPORTANT: Do this FIRST
    auto inFlightFence = m_frameManager.getInFlightFence();
    SPDLOG_DEBUG("Waiting for in-flight fence: {:#x}", reinterpret_cast<uint64_t>(inFlightFence));

    vkWaitForFences(
        m_vulkanContext.logical().get(),
        1,
        &inFlightFence,
        VK_TRUE,
        UINT64_MAX
    );

    // Now that we've waited for the fence, we can safely reset it
    vkResetFences(m_vulkanContext.logical().get(), 1, &inFlightFence);
    SPDLOG_DEBUG("Fence reset");

    // Zwolnij framebuffer z poprzedniej klatki jeśli istnieje
    if (currentFrame.framebufferHandle.id != 0) {
        SPDLOG_DEBUG("Releasing previous frame's framebuffer handle: {}", currentFrame.framebufferHandle.id);
        m_framebufferManager.removeReference(currentFrame.framebufferHandle);
        currentFrame.framebufferHandle = FrameBufferHandle(0); // Reset handle
    }

    // Reset command buffers after fence wait
    if (currentFrame.graphicsCommandBuffer) {
        // Check if the command buffer is in recording state and end it first
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

    if (currentFrame.transferCommandBuffer) {
        // Check if the command buffer is in recording state and end it first
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

    // Execute pending transfers from TransferManager if any
    if (m_vramManager.transferManager().hasPendingTransfers()) {
        SPDLOG_INFO("Executing pending VRAM transfers");
        // Execute transfers using both command buffers (transfer for the actual transfers,
        // graphics for the post-transfer barriers)
        m_vramManager.transferManager().executeTransfers(
            *currentFrame.transferCommandBuffer,
            *currentFrame.graphicsCommandBuffer
        );
        currentFrame.hasTransferCommands = true;
    }

    // Execute transfer commands if any were recorded
    if (currentFrame.hasTransferCommands) {
        SPDLOG_DEBUG("Submitting transfer commands");
        // End the transfer command buffer only if it's still recording
        if (currentFrame.transferCommandBuffer->isRecording()) {
            currentFrame.transferCommandBuffer->end();
        }

        // Submit transfer commands
        VkSemaphore signalSemaphore = currentFrame.transferFinished;
        currentFrame.transferCommandBuffer->submit(
            m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Transfer),
            {}, // No wait semaphores
            {}, // No wait stages
            std::span<const VkSemaphore>(&signalSemaphore, 1), // Signal transfer finished semaphore
            VK_NULL_HANDLE // No fence needed, we'll wait for render fence later
        );
    }

    // Reclaim staging buffers that are no longer in use
    m_vramManager.reclaimStagingBuffers();

    // Acquire the next image from the swap chain
    uint32_t imageIndex;
    SPDLOG_DEBUG("Acquiring next swapchain image");
    VkResult result = m_swapChain.acquireNextImage(
        m_frameManager.getImageAvailableSemaphore(),
        &imageIndex
    );
    SPDLOG_INFO("Acquired swapchain image: index={}, result={}", imageIndex, static_cast<int>(result));

    // Handle swapchain recreation - FIXED DEADLOCK ISSUE
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        SPDLOG_WARN("Swapchain out of date or suboptimal, recreating");

        // Properly end command buffers before recreating swapchain
        if (currentFrame.graphicsCommandBuffer && currentFrame.graphicsCommandBuffer->isRecording()) {
            currentFrame.graphicsCommandBuffer->end();
        }
        if (currentFrame.transferCommandBuffer && currentFrame.transferCommandBuffer->isRecording()) {
            currentFrame.transferCommandBuffer->end();
        }

        // Clear render orders before returning
        currentFrame.renderOrders.clear();
        currentFrame.hasTransferCommands = false;

        // CRITICAL FIX: Signal the fence before returning to prevent deadlock
        // This ensures the next frame won't wait indefinitely
        SPDLOG_DEBUG("Signaling fence before swapchain recreation to prevent deadlock");

        // First check if fence is already signaled
        VkResult fenceStatus = vkGetFenceStatus(m_vulkanContext.logical().get(), inFlightFence);
        if (fenceStatus == VK_NOT_READY) {
            // Submit an empty command buffer to signal the fence
            auto tempCmdBuffer = currentFrame.graphicsCommandBuffer.get();
            if (!tempCmdBuffer->isRecording()) {
                tempCmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            }
            tempCmdBuffer->end();

            // Submit with fence to signal it
            tempCmdBuffer->submit(
                m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics),
                {}, // No wait semaphores
                {}, // No wait stages  
                {}, // No signal semaphores
                inFlightFence // Signal this fence
            );

            SPDLOG_DEBUG("Empty command buffer submitted to signal fence");
        }

        // Wait for queues to be idle
        vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics));
        vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Transfer));

        // Now safely recreate the swapchain
        m_renderer.recreateSwapChain();
        m_depthAttachmentHandle = m_renderer.depthAttachmentHandle(); // Refresh depth attachment handle
        m_msColorAttachmentHandle = m_renderer.msColorAttachmentHandle(); // Refresh MSAA color attachment handle
        m_mainRenderPassHandle = m_renderer.renderPass(); // Refresh main render pass handle

        // Advance frame counter to keep synchronization consistent
        m_frameManager.advanceFrame();
        return;
    }
    else if (result != VK_SUCCESS) {
        // End command buffers before throwing
        SPDLOG_ERROR("Failed to acquire swap chain image! Result: {}", static_cast<int>(result));
        if (currentFrame.graphicsCommandBuffer && currentFrame.graphicsCommandBuffer->isRecording()) {
            currentFrame.graphicsCommandBuffer->end();
        }
        if (currentFrame.transferCommandBuffer && currentFrame.transferCommandBuffer->isRecording()) {
            currentFrame.transferCommandBuffer->end();
        }

        // Signal fence before throwing to prevent deadlock on next call
        VkResult fenceStatus = vkGetFenceStatus(m_vulkanContext.logical().get(), inFlightFence);
        if (fenceStatus == VK_NOT_READY) {
            // Signal fence to prevent deadlock
            auto tempCmdBuffer = currentFrame.graphicsCommandBuffer.get();
            if (!tempCmdBuffer->isRecording()) {
                tempCmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            }
            tempCmdBuffer->end();
            tempCmdBuffer->submit(
                m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics),
                {}, {}, {}, inFlightFence
            );
            vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics));
        }

        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    VramHandle swapchainImageHandle = m_swapChain.getImageHandles()[imageIndex];
    SPDLOG_DEBUG("Using swapchain image handle: {}", swapchainImageHandle.id);

    // Get or create framebuffer for this swapchain image
    AttachmentHandle swapchainAttachmentHandle = m_attachmentManager.registerExternalImage(
        swapchainImageHandle,
        m_swapChain.getImageFormat(),
        m_swapChain.getSwapChainExtent(),
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        AttachmentType::Resolve
    );
    SPDLOG_DEBUG("Created swapchain attachment handle: {}", swapchainAttachmentHandle.id);

    // Need to use msaa color attachment, resolve attachment, and depth
    std::vector<AttachmentHandle> framebufferAttachments = {
        m_msColorAttachmentHandle, // First comes the multisampled color attachment
        swapchainAttachmentHandle, // Then the resolve attachment (swapchain image)
        m_depthAttachmentHandle    // Finally the depth attachment
    };
    SPDLOG_DEBUG("Using MSAA color attachment handle: {}", m_msColorAttachmentHandle.id);
    SPDLOG_DEBUG("Using depth attachment handle: {}", m_depthAttachmentHandle.id);

    // Acquire framebuffer i zapisz handle w current frame
    FrameBufferHandle framebufferHandle = m_framebufferManager.acquireFrameBuffer(
        m_mainRenderPassHandle,
        framebufferAttachments,
        m_swapChain.getSwapChainExtent()
    );

    // Zapisz handle w current frame do późniejszego zwolnienia
    currentFrame.framebufferHandle = framebufferHandle;
    SPDLOG_DEBUG("Using framebuffer handle: {}", framebufferHandle.id);

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = *m_renderPassManager.getResource(m_mainRenderPassHandle);
    renderPassInfo.framebuffer = m_framebufferManager.getResource(framebufferHandle)->frameBuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_swapChain.getSwapChainExtent();

    // Clear values for attachments (now we have 3 attachments)
    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 0.0f} }; // Black background for MSAA color
    clearValues[1].color = { {0.0f, 0.0f, 0.0f, 1.0f} }; // Black for resolve (will be overwritten)
    clearValues[2].depthStencil = { 1.0f, 0 };          // Depth clear value

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // Begin render pass
    SPDLOG_INFO("Beginning render pass with dimensions: {}x{}",
        m_swapChain.getSwapChainExtent().width,
        m_swapChain.getSwapChainExtent().height);

    vkCmdBeginRenderPass(
        currentFrame.graphicsCommandBuffer.get()->handle(),
        &renderPassInfo,
        VK_SUBPASS_CONTENTS_INLINE
    );

    // Execute rendering commands for all collected render orders
    executeRenderCommands(
        currentFrame.graphicsCommandBuffer.get()->handle(),
        imageIndex,
        currentFrame.renderOrders
    );

    // End render pass
    vkCmdEndRenderPass(currentFrame.graphicsCommandBuffer.get()->handle());
    SPDLOG_DEBUG("Render pass ended");

    // Make sure we end the graphics command buffer only if it's still recording
    if (currentFrame.graphicsCommandBuffer->isRecording()) {
        currentFrame.graphicsCommandBuffer->end();
    }

    // Prepare wait semaphores and stages
    std::vector<VkSemaphore> waitSemaphores = {
        m_frameManager.getImageAvailableSemaphore()  // Always wait for image to be available
    };
    std::vector<VkPipelineStageFlags> waitStages = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    // Only wait for transfer if transfers were actually submitted
    if (currentFrame.hasTransferCommands) {
        waitSemaphores.push_back(m_frameManager.gettransferFinishedSemaphore());
        waitStages.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
        SPDLOG_DEBUG("Added transfer semaphore to wait list");
    }

    std::vector<VkSemaphore> signalSemaphores = { m_frameManager.getRenderFinishedSemaphore() };

    // Add try-catch block to handle errors during submission
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
        // Log the error but continue
        SPDLOG_ERROR("Error submitting graphics command buffer: {}", e.what());

        // Clear render orders for next frame
        currentFrame.renderOrders.clear();
        currentFrame.hasTransferCommands = false;

        // Advance to the next frame
        m_frameManager.advanceFrame();
        return;
    }

    // Present the image to the swapchain
    SPDLOG_DEBUG("Presenting image index: {}", imageIndex);
    result = m_swapChain.presentImage(imageIndex, m_frameManager.getRenderFinishedSemaphore());
    SPDLOG_INFO("Present result: {}", static_cast<int>(result));

    // Handle swapchain recreation after present - ALSO FIXED
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        SPDLOG_WARN("Swapchain out of date or suboptimal after present, recreating");

        // Clear render orders before recreation
        currentFrame.renderOrders.clear();
        currentFrame.hasTransferCommands = false;

        // Wait for queues instead of calling waitForAllFrames
        vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Graphics));
        vkQueueWaitIdle(m_vulkanContext.logical().getQueue(LogicalDevice::QueueType::Transfer));

        // Safely recreate swapchain
        m_renderer.recreateSwapChain();
        m_depthAttachmentHandle = m_renderer.depthAttachmentHandle(); // Refresh depth attachment handle
        m_msColorAttachmentHandle = m_renderer.msColorAttachmentHandle(); // Refresh MSAA color attachment handle
        m_mainRenderPassHandle = m_renderer.renderPass(); // Refresh main render pass handle

        // Advance frame counter
        m_frameManager.advanceFrame();
        return;
    }
    else if (result != VK_SUCCESS) {
        SPDLOG_ERROR("Failed to present swap chain image! Result: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to present swap chain image!");
    }

    // Clear render orders for next frame
    currentFrame.renderOrders.clear();
    currentFrame.hasTransferCommands = false;

    // Advance to the next frame
    m_frameManager.advanceFrame();
    SPDLOG_DEBUG("Advanced to next frame");
}

void RenderStage::executeRenderCommands(
    VkCommandBuffer commandBuffer,
    uint32_t imageIndex,
    const std::vector<std::shared_ptr<RenderOrder>>& renderOrders
) {
    SPDLOG_INFO("Executing render commands for {} orders", renderOrders.size());

    // Process mesh orders to render geometry
    for (const auto& order : renderOrders) {
        if (order->getType() == RenderOrderType::Mesh) {
            auto meshOrder = static_cast<MeshRenderOrder*>(order.get());
            SPDLOG_DEBUG("Processing mesh render order, pipeline handle: {}, mesh handle: {}",
                meshOrder->pipelineHandle.id, meshOrder->meshHandle.id);

            Pipeline* pipeline = &m_pipelineManager.get(meshOrder->pipelineHandle);
            if (!pipeline) {
                SPDLOG_ERROR("Failed to get pipeline from handle: {}", meshOrder->pipelineHandle.id);
                continue;
            }

            // Bind the pipeline
            vkCmdBindPipeline(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline->get()
            );
            SPDLOG_DEBUG("Pipeline bound");

            // Set dynamic viewport and scissor
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
            SPDLOG_DEBUG("Viewport and scissor set: {}x{}", viewport.width, viewport.height);

            // Bind descriptor sets using handles
            if (meshOrder->globalDescriptorSetHandle.isValid()) {
                VkDescriptorSet globalDescSet = m_descriptorAllocator.getDescriptorSet(meshOrder->globalDescriptorSetHandle.handle());
                vkCmdBindDescriptorSets(
                    commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getLayout(),
                    0, // First set
                    1, // Set count
                    &globalDescSet,
                    0, nullptr
                );
                SPDLOG_DEBUG("Global descriptor set bound, handle ID: {}",
                    meshOrder->globalDescriptorSetHandle.handle().id);
            }
            else {
                SPDLOG_WARN("Global descriptor set handle is invalid");
            }

            if (meshOrder->objectDescriptorSetHandle.isValid()) {
                VkDescriptorSet objectDescSet = m_descriptorAllocator.getDescriptorSet(meshOrder->objectDescriptorSetHandle.handle());
                vkCmdBindDescriptorSets(
                    commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getLayout(),
                    1, // Second set
                    1, // Set count
                    &objectDescSet,
                    0, nullptr
                );
                SPDLOG_DEBUG("Object descriptor set bound, handle ID: {}",
                    meshOrder->objectDescriptorSetHandle.handle().id);
            }
            else {
                SPDLOG_WARN("Object descriptor set handle is invalid");
            }

            if (meshOrder->materialDescriptorSetHandle.isValid()) {
                VkDescriptorSet materialDescSet = m_descriptorAllocator.getDescriptorSet(meshOrder->materialDescriptorSetHandle.handle());
                vkCmdBindDescriptorSets(
                    commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getLayout(),
                    2, // Third set
                    1, // Set count
                    &materialDescSet,
                    0, nullptr
                );
                SPDLOG_DEBUG("Material descriptor set bound, handle ID: {}",
                    meshOrder->materialDescriptorSetHandle.handle().id);
            }
            else {
                SPDLOG_WARN("Material descriptor set handle is invalid");
            }

            // Bind vertex and index buffers
            const Mesh* mesh = m_meshManager.getMesh(meshOrder->meshHandle);
            if (mesh) {
                SPDLOG_DEBUG("Using mesh: handle={}, vertices={}, indices={}",
                    meshOrder->meshHandle.id, mesh->vertexCount, mesh->indexCount);

                VkBuffer vertexBuffer = m_vramManager.getResource<Buffer>(mesh->vertexBuffer)->get();
                VkBuffer indexBuffer = m_vramManager.getResource<Buffer>(mesh->indexBuffer)->get();

                if (vertexBuffer == VK_NULL_HANDLE) {
                    SPDLOG_ERROR("Vertex buffer handle is NULL!");
                }

                if (indexBuffer == VK_NULL_HANDLE) {
                    SPDLOG_ERROR("Index buffer handle is NULL!");
                }

                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
                SPDLOG_DEBUG("Vertex buffer bound: {:#x}", reinterpret_cast<uint64_t>(vertexBuffer));

                // Proper index buffer binding and validation
                VkIndexType vkIndexType = (mesh->indexType == 0) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, vkIndexType);
                SPDLOG_DEBUG("Index buffer bound: {:#x}, type: {}",
                    reinterpret_cast<uint64_t>(indexBuffer),
                    (vkIndexType == VK_INDEX_TYPE_UINT16 ? "UINT16" : "UINT32"));

                // Validate index buffer size
                size_t indexSize = mesh->getIndexSize();
                VkDeviceSize expectedSize = indexSize * mesh->indexCount;
                VkDeviceSize actualSize = m_vramManager.getResource<Buffer>(mesh->indexBuffer)->getSize();
                SPDLOG_DEBUG("Index buffer: expected size={}, actual size={}, indexSize={}",
                    expectedSize, actualSize, indexSize);

                if (expectedSize > actualSize) {
                    SPDLOG_ERROR("Index buffer too small: expected {} bytes, got {} bytes",
                        expectedSize, actualSize);
                    // Calculate safe count
                    uint32_t safeCount = static_cast<uint32_t>(actualSize / indexSize);
                    vkCmdDrawIndexed(
                        commandBuffer,
                        safeCount,
                        1, // Instance count
                        0, // First index
                        0, // Vertex offset
                        0  // First instance
                    );
                    SPDLOG_WARN("Drawing with reduced index count: {}", safeCount);
                }
                else {
                    // Draw indexed
                    vkCmdDrawIndexed(
                        commandBuffer,
                        mesh->indexCount,
                        1, // Instance count
                        0, // First index
                        0, // Vertex offset
                        0  // First instance
                    );
                    SPDLOG_INFO("Drawing indexed: count={}", mesh->indexCount);
                }
            }
            else {
                SPDLOG_ERROR("Mesh not found for handle: {}", meshOrder->meshHandle.id);
            }
        }
        else {
            SPDLOG_WARN("Unsupported render order type: {}", static_cast<int>(order->getType()));
        }
    }
}