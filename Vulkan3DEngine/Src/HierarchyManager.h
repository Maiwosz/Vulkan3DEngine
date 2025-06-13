#pragma once
#include "Entity.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

class HierarchyManager {
public:
    // Ustawia rodzica dla entity
    void setParent(Entity child, Entity parent);

    // Usuwa rodzica (entity staje się root)
    void removeParent(Entity child);

    // Dodaje dziecko do rodzica
    void addChild(Entity parent, Entity child);

    // Usuwa dziecko od rodzica
    void removeChild(Entity parent, Entity child);

    // Sprawdza czy entity ma rodzica
    bool hasParent(Entity entity) const;

    // Zwraca rodzica entity (jeśli ma)
    Entity getParent(Entity entity) const;

    // Zwraca dzieci entity
    const std::unordered_set<Entity>& getChildren(Entity entity) const;

    // Zwraca wszystkie dzieci (rekurencyjnie)
    std::vector<Entity> getAllChildren(Entity entity) const;

    // Zwraca wszystkich rodziców (ścieżka do root)
    std::vector<Entity> getParentChain(Entity entity) const;

    // Sprawdza czy entity jest dzieckiem (rekurencyjnie) innego entity
    bool isDescendantOf(Entity child, Entity ancestor) const;

    // Zwraca wszystkie root entities (bez rodziców)
    std::vector<Entity> getRootEntities() const;

    // Usuwa entity z hierarchii (używane gdy entity jest niszczone)
    void removeEntity(Entity entity);

    // Zwraca głębokość entity w hierarchii (0 = root)
    int getDepth(Entity entity) const;

private:
    std::unordered_map<Entity, Entity> m_parentMap;                    // child -> parent
    std::unordered_map<Entity, std::unordered_set<Entity>> m_childrenMap; // parent -> children

    // Pusta lista dzieci do zwracania referencji
    static const std::unordered_set<Entity> s_emptyChildren;

    // Rekurencyjne pomocnicze funkcje
    void getAllChildrenRecursive(Entity entity, std::vector<Entity>& result) const;
};