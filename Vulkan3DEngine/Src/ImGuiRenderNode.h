#pragma once
#include "RenderNode.h"
#include "ImGuiCore.h"

/**
 * Specialized render node for ImGui rendering.
 * Simplified - delegates ImGui backend management to ImGuiCore.
 */
class ImGuiRenderNode : public RenderNode {
public:
    ImGuiRenderNode(
        const RenderNodeTemplate* nodeTemplate,
        const RenderTarget& renderTarget,
        VkExtent2D extent,
        SmartRenderPassHandle smartRenderPassHandle,
        ImGuiCore& imguiCore,
        VkSampleCountFlagBits msaaSamples
    );

    virtual ~ImGuiRenderNode();

    // Override to handle ImGui rendering
    void beginRenderPass(VkCommandBuffer commandBuffer,
        VkFramebuffer framebuffer,
        const VkExtent2D& renderArea) const override;

    void endRenderPass(VkCommandBuffer commandBuffer) const override;

    // Render ImGui draw data
    void renderImGui(VkCommandBuffer commandBuffer);

private:
    ImGuiCore& m_imguiCore;
    VkSampleCountFlagBits m_msaaSamples;
    std::unique_ptr<class ImGuiRenderPass> m_imguiRenderPass;
};