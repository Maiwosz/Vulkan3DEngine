#pragma once

#include <memory>
#include <unordered_set>
#include <spdlog/spdlog.h>
#include "Entity.h"

// Forward declarations
class Registry;

class HierarchyWindow {
public:
    explicit HierarchyWindow(Registry& registry);
    ~HierarchyWindow() = default;

    // Main render function
    void render();

private:
    void renderEntityHierarchy();
    void renderEntityNode(Entity entity, int depth = 0);
    void renderRootEntities();

    bool isEntityExpanded(Entity entity) const;
    void setEntityExpanded(Entity entity, bool expanded);

    // Helper methods
    std::string getEntityDisplayName(Entity entity) const;
    bool hasChildren(Entity entity) const;
    void handleEntitySelection(Entity entity);
    void handleEntityContextMenu(Entity entity);

public:
    // UI State - public so EditorUI can access
    bool m_showWindow = true;

private:
    Registry& m_registry;

    // UI State
    std::unordered_set<Entity> m_expandedEntities;
    Entity m_selectedEntity{ 0 };

    std::shared_ptr<spdlog::logger> m_logger;
};