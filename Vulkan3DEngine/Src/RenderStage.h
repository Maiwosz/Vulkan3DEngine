#pragma once
#include "Prerequisites.h"
#include "ProcessingStage.h"
#include "RenderOrder.h"
#include "Renderer.h"
#include "AssetSystem.h"
#include "VulkanContext.h"
#include "FrameManager.h"
#include "VramManager.h"
#include "SwapChain.h"
#include "AttachmentManager.h"
#include "FrameBufferManager.h"
#include "RenderPassManager.h"
#include "PipelineManager.h"
#include "MeshManager.h"
#include "DescriptorAllocator.h"
#include "ImGuiWrapper.h"
#include <memory>
#include <vector>
#include <functional>

class RenderStage: public OrderProcessingStage {
public:
    using UIRenderCallback = std::function<void()>;

    RenderStage(Renderer& renderer, AssetSystem& assetSystem);
    ~RenderStage() = default;

    void process(std::shared_ptr<RenderOrder> order);
    void executeRenderPass();

    void setUIRenderCallback(UIRenderCallback callback) { m_uiRenderCallback = callback; }
private:
    // References to renderer components
    Renderer& m_renderer;
    VulkanContext& m_vulkanContext;
    FrameManager& m_frameManager;
    VramManager& m_vramManager;
    SwapChain& m_swapChain;
    AttachmentManager& m_attachmentManager;
    FrameBufferManager& m_framebufferManager;
    RenderPassManager& m_renderPassManager;
    PipelineManager& m_pipelineManager;
    MeshManager& m_meshManager;
    DescriptorAllocator& m_descriptorAllocator;

    // UI rendering callback
    UIRenderCallback m_uiRenderCallback;

    // Render pass and attachment handles
    AttachmentHandle m_depthAttachmentHandle;
    AttachmentHandle m_msColorAttachmentHandle;
    RenderPassHandle m_mainRenderPassHandle;

    // Frame synchronization helpers
    void waitForPreviousFrame();
    void signalFenceToPreventDeadlock(VkFence fence);

    // Command buffer management
    void resetAndBeginCommandBuffers();
    void endCommandBuffersOnError();

    // Transfer operations
    void executeTransferOperations();

    // Swapchain operations
    uint32_t acquireSwapchainImage();
    void handleSwapchainRecreation();
    void presentImage(uint32_t imageIndex);

    // Framebuffer management
    void releaseCurrentFramebuffer();
    FrameBufferHandle createFramebufferForImage(uint32_t imageIndex);

    // Render pass execution
    void beginRenderPass(FrameBufferHandle framebufferHandle);
    void endRenderPass();

    // Command submission
    void submitGraphicsCommands();

    // Cleanup
    void cleanupFrame();

    // Render command execution
    void executeRenderCommands(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        const std::vector<std::shared_ptr<RenderOrder>>& renderOrders
    );

    void executeMeshRenderOrder(VkCommandBuffer commandBuffer, MeshRenderOrder* meshOrder);
    void bindPipeline(VkCommandBuffer commandBuffer, PipelineHandle pipelineHandle);
    void setViewportAndScissor(VkCommandBuffer commandBuffer);
    void bindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, MeshRenderOrder* meshOrder);
    void bindVertexAndIndexBuffers(VkCommandBuffer commandBuffer, const Mesh* mesh);
    void drawMesh(VkCommandBuffer commandBuffer, const Mesh* mesh);

    // UI rendering
    void renderUI(VkCommandBuffer commandBuffer);
};