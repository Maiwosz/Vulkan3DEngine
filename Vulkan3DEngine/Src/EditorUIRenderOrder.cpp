#include "EditorUIRenderOrder.h"
#include "Renderer.h"
#include "ImGuiWrapper.h"
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void EditorUIRenderOrder::execute(VkCommandBuffer commandBuffer, Renderer& renderer, AssetSystem& assetySystem) {
    SPDLOG_DEBUG("Executing EditorUI render order");

    try {
        // Begin new ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Execute UI callback if provided
        if (m_callback) {
            m_callback();
        }
        else {
            SPDLOG_DEBUG("No UI callback provided for EditorUIRenderOrder");
        }

        // Render UI using ImGuiWrapper
        renderer.imguiWrapper().render(commandBuffer);

        SPDLOG_DEBUG("EditorUI rendering completed");
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in EditorUIRenderOrder::execute: {}", e.what());
    }
}