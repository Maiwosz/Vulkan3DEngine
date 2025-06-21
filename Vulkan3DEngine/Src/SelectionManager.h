#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>
#include "Entity.h"

class SelectionManager {
public:
    using SelectionCallback = std::function<void(Entity)>;

    SelectionManager();
    ~SelectionManager() = default;

    // Selection management
    void selectEntity(Entity entity);
    void clearSelection();
    Entity getSelectedEntity() const { return m_selectedEntity; }
    bool hasSelection() const { return m_selectedEntity.id != 0; }

    // Callback management
    void addSelectionCallback(SelectionCallback callback);
    void removeAllCallbacks();

private:
    void notifySelectionChanged();

    Entity m_selectedEntity{ 0 };
    std::vector<SelectionCallback> m_selectionCallbacks;
};