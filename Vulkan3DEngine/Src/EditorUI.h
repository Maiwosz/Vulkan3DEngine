#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include "HierarchyWindow.h"

// Forward declarations
class Engine;
class Registry;
class FrameManager;

class EditorUI {
public:
    explicit EditorUI(Engine& engine);
    ~EditorUI();

    // Main update function - handles all UI logic and rendering
    void update();

    // Window access
    HierarchyWindow& hierarchyWindow() { return *m_hierarchyWindow; }

private:
    void initializeImGui();
    void beginFrame();
    void endFrame();
    void renderWindows();

private:
    Engine& m_engine;
    Registry& m_registry;
    FrameManager& m_frameManager;

    // UI Windows
    std::unique_ptr<HierarchyWindow> m_hierarchyWindow;

    std::shared_ptr<spdlog::logger> m_logger;
};