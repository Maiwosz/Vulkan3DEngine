#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include "HierarchyWindow.h"
#include "SelectionManager.h"
#include "ComponentInspectorWindow.h"

// Forward declarations
class Engine;
class Registry;
class FrameManager;

class EditorUI {
public:
    explicit EditorUI(Engine& engine);
    ~EditorUI();

    // Renders only ImGui commands - should be called from RenderStage callback
    void renderWindows();

    // Window access
    HierarchyWindow& hierarchyWindow() { return *m_hierarchyWindow; }
    ComponentInspectorWindow& componentInspectorWindow() { return *m_componentInspectorWindow; }

private:
    Engine& m_engine;
    Registry& m_registry;
    FrameManager& m_frameManager;

    // UI Windows
    std::unique_ptr<SelectionManager> m_selectionManager;
    std::unique_ptr<HierarchyWindow> m_hierarchyWindow;
    std::unique_ptr<ComponentInspectorWindow> m_componentInspectorWindow;
};