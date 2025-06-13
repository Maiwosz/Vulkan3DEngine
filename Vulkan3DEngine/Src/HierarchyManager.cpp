#include "HierarchyManager.h"
#include <algorithm>

const std::unordered_set<Entity> HierarchyManager::s_emptyChildren;

void HierarchyManager::setParent(Entity child, Entity parent) {
    // Sprawdź czy nie tworzymy cyklu
    if (isDescendantOf(parent, child)) {
        return; // Nie pozwalamy na cykle
    }

    // Usuń poprzedniego rodzica jeśli istnieje
    if (hasParent(child)) {
        removeParent(child);
    }

    // Ustaw nowego rodzica
    m_parentMap[child] = parent;
    m_childrenMap[parent].insert(child);
}

void HierarchyManager::removeParent(Entity child) {
    auto it = m_parentMap.find(child);
    if (it != m_parentMap.end()) {
        Entity parent = it->second;
        m_parentMap.erase(it);

        // Usuń z listy dzieci rodzica
        if (auto childrenIt = m_childrenMap.find(parent); childrenIt != m_childrenMap.end()) {
            childrenIt->second.erase(child);
            if (childrenIt->second.empty()) {
                m_childrenMap.erase(childrenIt);
            }
        }
    }
}

void HierarchyManager::addChild(Entity parent, Entity child) {
    setParent(child, parent);
}

void HierarchyManager::removeChild(Entity parent, Entity child) {
    auto it = m_parentMap.find(child);
    if (it != m_parentMap.end() && it->second == parent) {
        removeParent(child);
    }
}

bool HierarchyManager::hasParent(Entity entity) const {
    return m_parentMap.count(entity) > 0;
}

Entity HierarchyManager::getParent(Entity entity) const {
    auto it = m_parentMap.find(entity);
    return (it != m_parentMap.end()) ? it->second : Entity(0);
}

const std::unordered_set<Entity>& HierarchyManager::getChildren(Entity entity) const {
    auto it = m_childrenMap.find(entity);
    return (it != m_childrenMap.end()) ? it->second : s_emptyChildren;
}

std::vector<Entity> HierarchyManager::getAllChildren(Entity entity) const {
    std::vector<Entity> result;
    getAllChildrenRecursive(entity, result);
    return result;
}

void HierarchyManager::getAllChildrenRecursive(Entity entity, std::vector<Entity>& result) const {
    const auto& children = getChildren(entity);
    for (Entity child : children) {
        result.push_back(child);
        getAllChildrenRecursive(child, result);
    }
}

std::vector<Entity> HierarchyManager::getParentChain(Entity entity) const {
    std::vector<Entity> chain;
    Entity current = entity;

    while (hasParent(current)) {
        current = getParent(current);
        chain.push_back(current);
    }

    return chain;
}

bool HierarchyManager::isDescendantOf(Entity child, Entity ancestor) const {
    Entity current = child;

    while (hasParent(current)) {
        current = getParent(current);
        if (current == ancestor) {
            return true;
        }
    }

    return false;
}

std::vector<Entity> HierarchyManager::getRootEntities() const {
    std::vector<Entity> roots;

    // Zbierz wszystkie entity które mają dzieci ale nie mają rodziców
    for (const auto& [parent, children] : m_childrenMap) {
        if (!hasParent(parent)) {
            roots.push_back(parent);
        }
    }

    return roots;
}

void HierarchyManager::removeEntity(Entity entity) {
    // Usuń wszystkie dzieci (przenieś je do rodziców lub ustaw jako root)
    const auto& children = getChildren(entity);
    Entity parent = hasParent(entity) ? getParent(entity) : Entity(0);

    for (Entity child : children) {
        removeParent(child);
        if (parent.id != 0) {
            setParent(child, parent);
        }
    }

    // Usuń entity z hierarchii
    removeParent(entity);
    m_childrenMap.erase(entity);
}

int HierarchyManager::getDepth(Entity entity) const {
    int depth = 0;
    Entity current = entity;

    while (hasParent(current)) {
        current = getParent(current);
        depth++;
    }

    return depth;
}