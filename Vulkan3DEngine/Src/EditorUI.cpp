#include "EditorUI.h"
#include "Engine.h"
#include "Registry.h"
#include "FrameManager.h"
#include "Renderer.h"
#include "imgui.h"
#include "LoggerConfig.h"

EditorUI::EditorUI(Engine& engine)
    : m_engine(engine)
    , m_registry(engine.registry())
    , m_frameManager(engine.renderer().frameManager())
{
    EDITOR_LOG_INFO("Initializing EditorUI");

    // Create selection manager first
    m_selectionManager = std::make_unique<SelectionManager>();

    // Create UI windows with selection manager
    m_hierarchyWindow = std::make_unique<HierarchyWindow>(m_registry, *m_selectionManager);
    m_componentInspectorWindow = std::make_unique<ComponentInspectorWindow>(m_registry, *m_selectionManager);

    EDITOR_LOG_INFO("EditorUI initialized successfully");
}

EditorUI::~EditorUI() {
    EDITOR_LOG_INFO("EditorUI destroyed");
}

void EditorUI::renderWindows() {
    // Render main menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                EDITOR_LOG_INFO("New Scene requested");
                // TODO: Implement new scene creation
            }
            if (ImGui::MenuItem("Open Scene")) {
                EDITOR_LOG_INFO("Open Scene requested");
                // TODO: Implement scene loading
            }
            if (ImGui::MenuItem("Save Scene")) {
                EDITOR_LOG_INFO("Save Scene requested");
                // TODO: Implement scene saving
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                EDITOR_LOG_INFO("Exit requested");
                m_engine.requestShutdown();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows")) {
            ImGui::MenuItem("Hierarchy", nullptr, &m_hierarchyWindow->m_showWindow);
            ImGui::MenuItem("Component Inspector", nullptr, &m_componentInspectorWindow->m_showWindow);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // Render individual windows
    m_hierarchyWindow->render();
    m_componentInspectorWindow->render();

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

        if (m_registry.scenes().hasCurrentScene()) {
            ImGui::Text("Current Scene: %s", m_registry.scenes().getCurrentSceneName().c_str());
        }
        else {
            ImGui::Text("No scene loaded");
        }
    }
    ImGui::End();
}