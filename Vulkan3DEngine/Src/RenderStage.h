#pragma once
#include "ProcessingStage.h"
#include "FrameManager.h"
#include "VramManager.h"
#include "SwapChain.h"
#include "AttachmentManager.h"
#include "FrameBufferManager.h"
#include "RenderPassManager.h"
#include "VulkanContext.h"
#include <memory>
#include <vector>
#include "Renderer.h"

class RenderStage : public OrderProcessingStage {
public:
    RenderStage(
        Renderer& renderer
    );
    ~RenderStage() = default;

    // Process a single render order and collect it for rendering
    void process(std::shared_ptr<RenderOrder> order) override;

    // Execute all rendering for the current frame
    void executeRenderPass();

    void createAndRenderDebugObject();

private:
    VulkanContext& m_vulkanContext;
    FrameManager& m_frameManager;
    VramManager& m_vramManager;
    SwapChain& m_swapChain;
    AttachmentManager& m_attachmentManager;
    FrameBufferManager& m_framebufferManager;
    RenderPassManager& m_renderPassManager;
    PipelineManager& m_pipelineManager;
    MeshManager& m_meshManager;
    AttachmentHandle m_depthAttachmentHandle;
    RenderPassHandle m_mainRenderPassHandle;
    

    // Helper function to execute render commands for collected orders
    void executeRenderCommands(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        const std::vector<std::shared_ptr<RenderOrder>>& renderOrders
    );
};