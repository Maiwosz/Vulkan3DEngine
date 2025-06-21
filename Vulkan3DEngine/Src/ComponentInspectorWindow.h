#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>
#include <json.hpp>
#include "Entity.h"

// Forward declarations
class Registry;
class SelectionManager;

class ComponentInspectorWindow {
public:
    explicit ComponentInspectorWindow(Registry& registry, SelectionManager& selectionManager);
    ~ComponentInspectorWindow() = default;

    // Main render function
    void render();

    // Set the entity to inspect
    void setSelectedEntity(Entity entity);

private:
    void renderEntityInfo();
    void renderComponentList();
    void renderAddComponentSection();
    void renderComponentEditor(const std::string& componentName);

    // Component management
    void addComponent(const std::string& componentName);
    void removeComponent(const std::string& componentName);

public:
    // UI State - public so EditorUI can access
    bool m_showWindow = true;

private:
    Registry& m_registry;
    SelectionManager& m_selectionManager;

    // UI state
    std::string m_componentToAdd;
    bool m_showAddComponentPopup = false;
};