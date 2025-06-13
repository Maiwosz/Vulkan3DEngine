#include "Registry.h"

Registry::Registry()
    : m_systemManager(std::make_unique<SystemManager>(*this)) {
}

Entity Registry::create() {
    if (!m_freeEntities.empty()) {
        auto it = m_freeEntities.begin();
        Entity entity = *it;
        m_freeEntities.erase(it);
        m_entities.insert(entity);
        return entity;
    }
    Entity entity(m_nextId_++);
    m_entities.insert(entity);
    return entity;
}

void Registry::destroy(Entity entity) {
    if (m_entities.count(entity)) {
        // Usuń z hierarchii przed usunięciem komponentów
        m_hierarchyManager.removeEntity(entity);

        // Usuń wszystkie komponenty
        for (auto& [type, pool] : m_componentPools) {
            pool->remove(entity);
        }

        m_entities.erase(entity);
        m_freeEntities.insert(entity);
    }
}

bool Registry::valid(Entity entity) const {
    return m_entities.count(entity) > 0;
}