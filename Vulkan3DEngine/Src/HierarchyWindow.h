#pragma once

#include <memory>
#include <unordered_set>
#include <spdlog/spdlog.h>
#include "Entity.h"

// Forward declarations
class Registry;
class SelectionManager;
class PrefabInstanceManager;

class HierarchyWindow {
public:
    explicit HierarchyWindow(Registry& registry, SelectionManager& selectionManager);
    ~HierarchyWindow() = default;

    // Main render function
    void render();

private:
    void renderEntityHierarchy();
    void renderEntityNode(Entity entity, int depth = 0);
    void renderRootEntities();
    void renderPrefabMenu();

    bool isEntityExpanded(Entity entity) const;
    void setEntityExpanded(Entity entity, bool expanded);

    // Helper methods
    std::string getEntityDisplayName(Entity entity) const;
    bool hasChildren(Entity entity) const;
    void handleEntitySelection(Entity entity);
    void handleEntityContextMenu(Entity entity);
    std::vector<std::string> getAvailablePrefabs() const;
    std::string extractPrefabName(const std::string& filename) const;
public:
    // UI State - public so EditorUI can access
    bool m_showWindow = true;

private:
    Registry& m_registry;
    SelectionManager& m_selectionManager;

    // UI State
    std::unordered_set<Entity> m_expandedEntities;

    void handleEmptySpaceContextMenu();
    void renderPrefabMenuWithParent(Entity parent);
    void startRename(Entity entity);
    void handleRenameDialog();

    // Rename dialog state
    bool m_showRenameDialog = false;
    Entity m_renameEntity = Entity(0);
    char m_renameBuffer[256] = "";
};