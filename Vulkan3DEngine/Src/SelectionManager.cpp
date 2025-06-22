#include "SelectionManager.h"
#include "LoggerManager.h"

SelectionManager::SelectionManager() {
    EDITOR_LOG_DEBUG("SelectionManager initialized");
}

void SelectionManager::selectEntity(Entity entity) {
    if (m_selectedEntity != entity) {
        m_selectedEntity = entity;
        EDITOR_LOG_DEBUG("Entity {} selected", entity.id);
        notifySelectionChanged();
    }
}

void SelectionManager::clearSelection() {
    if (m_selectedEntity.id != 0) {
        m_selectedEntity = Entity(0);
        EDITOR_LOG_DEBUG("Selection cleared");
        notifySelectionChanged();
    }
}

void SelectionManager::addSelectionCallback(SelectionCallback callback) {
    m_selectionCallbacks.push_back(callback);
}

void SelectionManager::removeAllCallbacks() {
    m_selectionCallbacks.clear();
}

void SelectionManager::notifySelectionChanged() {
    for (const auto& callback : m_selectionCallbacks) {
        callback(m_selectedEntity);
    }
}