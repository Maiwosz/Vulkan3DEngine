#include "ImGuiRenderNode.h"
#include "ImGuiRenderPass.h"
#include "RenderPassManager.h"
#include <stdexcept>

ImGuiRenderNode::ImGuiRenderNode(
    const RenderNodeTemplate* nodeTemplate,
    const RenderTarget& renderTarget,
    VkExtent2D extent,
    SmartRenderPassHandle smartRenderPassHandle,
    ImGuiCore& imguiCore,
    VkSampleCountFlagBits msaaSamples
)
    : RenderNode(nodeTemplate, renderTarget, extent, std::move(smartRenderPassHandle))
    , m_imguiCore(imguiCore)
    , m_msaaSamples(msaaSamples)
{
    m_imguiRenderPass = m_imguiCore.createRenderPass(
        getSmartRenderPassHandle(),
        m_msaaSamples
    );
}

ImGuiRenderNode::~ImGuiRenderNode() {
    // m_imguiRenderPass destructor handles cleanup
}

void ImGuiRenderNode::beginRenderPass(VkCommandBuffer commandBuffer,
    VkFramebuffer framebuffer,
    const VkExtent2D& renderArea) const {
    RenderNode::beginRenderPass(commandBuffer, framebuffer, renderArea);
}

void ImGuiRenderNode::endRenderPass(VkCommandBuffer commandBuffer) const {
    RenderNode::endRenderPass(commandBuffer);
}

void ImGuiRenderNode::renderImGui(VkCommandBuffer commandBuffer) {
    if (!m_imguiRenderPass || !m_imguiRenderPass->isInitialized()) {
        throw std::runtime_error("ImGuiRenderNode::renderImGui called before initialization");
    }

    // Delegate to ImGuiRenderPass
    m_imguiRenderPass->render(commandBuffer);
}