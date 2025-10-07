#include "ImGuiDrawCall.h"
#include "ImGuiRenderNode.h"
#include "Renderer.h"
#include "imgui.h"
#include <stdexcept>

bool ImGuiDrawCall::execute(Renderer& renderer, EngineCore& engineCore, RenderNode& renderNode) {
    // Verify we're working with ImGuiRenderNode
    ImGuiRenderNode* imguiNode = dynamic_cast<ImGuiRenderNode*>(&renderNode);
    if (!imguiNode) {
        throw std::runtime_error("ImGuiDrawCall requires ImGuiRenderNode");
    }

    // Execute ImGui commands (this builds the draw data)
    if (m_callback) {
        m_callback();
    }

    // Note: Actual rendering to command buffer happens separately
    // via ImGuiRenderNode::renderImGui() after all UI is defined
    return true;
}