#pragma once

#include <memory>
#include <vector>
#include <string>
#include <spdlog/spdlog.h>
#include "HierarchyWindow.h"
#include "SelectionManager.h"
#include "ComponentInspectorWindow.h"
#include "MaterialCreatorUI.h"
#include "SettingsWindow.h"

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
    //MaterialCreatorUI& materialCreatorWindow() { return *m_materialCreatorWindow; }

private:
    Engine& m_engine;
    Registry& m_registry;
    FrameManager& m_frameManager;

    // UI Windows
    std::unique_ptr<SelectionManager> m_selectionManager;
    std::unique_ptr<HierarchyWindow> m_hierarchyWindow;
    std::unique_ptr<ComponentInspectorWindow> m_componentInspectorWindow;
    //std::unique_ptr<MaterialCreatorUI> m_materialCreatorWindow;
    std::unique_ptr<SettingsWindow> m_settingsWindow;

    // Scene management UI state
    bool m_showOpenSceneDialog;
    bool m_showSaveSceneDialog;
    std::vector<std::string> m_availableScenes;
    int m_selectedSceneIndex;
    char m_sceneNameBuffer[256];

    // Scene management functions
    void createNewScene();
    void saveCurrentScene();
    void refreshSceneList();
    void renderOpenSceneDialog();
    void renderSaveSceneDialog();
    void loadScene(const std::string& sceneName);
    void saveSceneAs(const std::string& sceneName);
};