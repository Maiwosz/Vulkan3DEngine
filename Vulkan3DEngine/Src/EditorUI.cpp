#include "EditorUI.h"
#include "Engine.h"
#include "Registry.h"
#include "FrameManager.h"
#include "Renderer.h"
#include "ImGuiWrapper.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

EditorUI::EditorUI(Engine& engine)
    : m_engine(engine)
    , m_registry(engine.registry())
    , m_frameManager(engine.renderer().frameManager())
{
    // Initialize logger
    m_logger = spdlog::get("EDITOR");
    if (!m_logger) {
        m_logger = spdlog::default_logger();
    }

    m_logger->info("Initializing EditorUI");

    // Create UI windows
    m_hierarchyWindow = std::make_unique<HierarchyWindow>(m_registry);

    m_logger->info("EditorUI initialized successfully");
}

EditorUI::~EditorUI() {
    m_logger->info("EditorUI destroyed");
}

void EditorUI::update() {
    beginFrame();
    renderWindows();
    endFrame();
}

void EditorUI::beginFrame() {
    // Start new ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorUI::renderWindows() {
    // Render main menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                m_logger->info("New Scene requested");
                // TODO: Implement new scene creation
            }
            if (ImGui::MenuItem("Open Scene")) {
                m_logger->info("Open Scene requested");
                // TODO: Implement scene loading
            }
            if (ImGui::MenuItem("Save Scene")) {
                m_logger->info("Save Scene requested");
                // TODO: Implement scene saving
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                m_logger->info("Exit requested");
                m_engine.requestShutdown();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows")) {
            ImGui::MenuItem("Hierarchy", nullptr, &m_hierarchyWindow->m_showWindow);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // Render individual windows
    m_hierarchyWindow->render();

    // Render engine stats window
    if (ImGui::Begin("Engine Stats")) {
        ImGui::Text("FPS: %.1f", m_engine.fps());
        ImGui::Text("Frame Time: %.3f ms", m_engine.deltaTime() * 1000.0f);
        ImGui::Text("Total Time: %.2f s", m_engine.totalTime());
        ImGui::Text("Frame Count: %llu", m_engine.frameCount());

        ImGui::Separator();

        // Registry stats
        auto rootEntities = m_registry.entities().getRootEntities();
        ImGui::Text("Root Entities: %zu", rootEntities.size());
        ImGui::Text("Total Entities: %zu", m_registry.entities().getAllEntities().size());

        if (m_registry.hasCurrentScene()) {
            ImGui::Text("Current Scene: %s", m_registry.getCurrentSceneName().c_str());
        }
        else {
            ImGui::Text("No scene loaded");
        }
    }
    ImGui::End();
}

void EditorUI::endFrame() {
    // Get current frame's command buffer
    auto& currentFrame = m_frameManager.getCurrentFrame();

    if (currentFrame.graphicsCommandBuffer && currentFrame.graphicsCommandBuffer->isRecording()) {
        // Render ImGui
        ImGui::Render();

        // Get the command buffer handle
        VkCommandBuffer commandBuffer = currentFrame.graphicsCommandBuffer->handle();

        // Render ImGui draw data to command buffer
        m_engine.renderer().imguiWrapper().render(commandBuffer);

        m_logger->debug("ImGui rendered to command buffer");
    }
    else {
        m_logger->warn("Graphics command buffer not available for ImGui rendering");
        // Still need to end the ImGui frame even if we can't render
        ImGui::Render();
    }
}