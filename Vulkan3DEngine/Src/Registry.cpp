#include "Registry.h"
#include <stdexcept>
#include <spdlog/spdlog.h>

Registry::Registry()
    : m_systemManager(std::make_unique<SystemManager>(*this)),
    m_componentManager(std::make_unique<ComponentManager>(*this)),
    m_hierarchyManager(std::make_unique<HierarchyManager>()),
    m_prefabInstanceManager(std::make_unique<PrefabInstanceManager>(*this)),
    m_serializer(std::make_unique<Serializer>(*this))
{
}

Entity Registry::create(Entity parent, const std::string& name) {
    Entity entity;

    if (!m_freeEntities.empty()) {
        auto it = m_freeEntities.begin();
        entity = *it;
        m_freeEntities.erase(it);
    }
    else {
        entity = Entity(m_nextId++);
    }

    m_entities.insert(entity);

    // Ustaw nazwę dla entity
    std::string finalName = name.empty() ? generateUniqueEntityName() : name;
    if (entityNameExists(finalName)) {
        finalName = generateUniqueEntityName(finalName);
    }

    m_entityNames[entity] = finalName;
    m_nameToEntity[finalName] = entity;

    if (parent.id != 0 && valid(parent)) {
        setParent(entity, parent);
    }

    return entity;
}

void Registry::destroy(Entity entity) {
    if (!m_entities.count(entity)) return;

    // Check if entity can be destroyed (not part of locked prefab instance)
    if (!m_prefabInstanceManager->canDestroyEntity(entity)) {
        SPDLOG_WARN("Cannot destroy entity {} - it's part of a prefab instance", entity.id);
        return;
    }

    // Notify prefab instance manager
    m_prefabInstanceManager->onEntityDestroyed(entity);

    // Usuń mapping nazwy
    auto nameIt = m_entityNames.find(entity);
    if (nameIt != m_entityNames.end()) {
        m_nameToEntity.erase(nameIt->second);
        m_entityNames.erase(nameIt);
    }

    m_hierarchyManager->removeEntity(entity);

    for (auto& [type, pool] : m_componentPools) {
        pool->remove(entity);
    }

    m_entities.erase(entity);
    m_freeEntities.insert(entity);
}

bool Registry::valid(Entity entity) const {
    return m_entities.count(entity) > 0;
}

std::string Registry::getEntityName(Entity entity) const {
    auto it = m_entityNames.find(entity);
    return (it != m_entityNames.end()) ? it->second : "";
}

void Registry::setEntityName(Entity entity, const std::string& name) {
    if (!valid(entity)) return;

    // Usuń stary mapping
    auto oldNameIt = m_entityNames.find(entity);
    if (oldNameIt != m_entityNames.end()) {
        m_nameToEntity.erase(oldNameIt->second);
    }

    // Sprawdź czy nazwa już istnieje
    std::string finalName = name;
    if (entityNameExists(finalName)) {
        finalName = generateUniqueEntityName(finalName);
    }

    // Ustaw nowy mapping
    m_entityNames[entity] = finalName;
    m_nameToEntity[finalName] = entity;
}

Entity Registry::findEntityByName(const std::string& name) const {
    auto it = m_nameToEntity.find(name);
    return (it != m_nameToEntity.end()) ? it->second : Entity(0);
}

bool Registry::entityNameExists(const std::string& name) const {
    return m_nameToEntity.find(name) != m_nameToEntity.end();
}

// Hierarchy helpers
void Registry::setParent(Entity child, Entity parent) {
    m_hierarchyManager->setParent(child, parent);
}

void Registry::removeParent(Entity child) {
    m_hierarchyManager->removeParent(child);
}

Entity Registry::getParent(Entity entity) const {
    return m_hierarchyManager->getParent(entity);
}

const std::unordered_set<Entity>& Registry::getChildren(Entity entity) const {
    return m_hierarchyManager->getChildren(entity);
}

bool Registry::hasParent(Entity entity) const {
    return m_hierarchyManager->hasParent(entity);
}

// Entity operations
Entity Registry::cloneEntity(Entity sourceEntity, Entity newParent, const std::string& name) {
    if (!valid(sourceEntity)) {
        return Entity(0);
    }

    std::string baseName = name.empty() ? (getEntityName(sourceEntity) + "_Copy") : name;
    Entity newEntity = create(newParent, baseName);

    auto componentTypes = getEntityComponentTypes(sourceEntity);
    for (const std::string& typeName : componentTypes) {
        auto componentData = m_serializer->serializeComponent(sourceEntity, typeName);
        m_serializer->deserializeComponent(newEntity, typeName, componentData);
    }

    const auto& children = getChildren(sourceEntity);
    for (Entity child : children) {
        cloneEntity(child, newEntity);
    }

    return newEntity;
}

std::vector<std::string> Registry::getEntityComponentTypes(Entity entity) const {
    std::vector<std::string> types;

    for (const auto& [typeIndex, pool] : m_componentPools) {
        if (pool->hasEntity(entity)) {
            try {
                std::string typeName = m_componentManager->getComponentName(typeIndex);
                types.push_back(typeName);
            }
            catch (const std::exception& e) {
                SPDLOG_WARN("Failed to get component type name: {}", e.what());
            }
        }
    }

    return types;
}

bool Registry::hasAnyComponents(Entity entity) const {
    for (const auto& [typeIndex, pool] : m_componentPools) {
        if (pool->hasEntity(entity)) {
            return true;
        }
    }
    return false;
}

// Private helper methods
Registry::IComponentPool* Registry::getComponentPool(const std::string& componentTypeName) const {
    try {
        std::type_index typeIndex = m_componentManager->getComponentType(componentTypeName);
        auto it = m_componentPools.find(typeIndex);
        return (it != m_componentPools.end()) ? it->second.get() : nullptr;
    }
    catch (const std::exception&) {
        return nullptr;
    }
}

std::string Registry::generateUniqueEntityName(const std::string& baseName) const {
    std::string base = baseName.empty() ? "Entity" : baseName;
    std::string candidate = base;

    int counter = 1;
    while (entityNameExists(candidate)) {
        candidate = base + "_" + std::to_string(counter);
        counter++;
    }

    return candidate;
}

uint32_t Registry::countEntitiesInHierarchy(Entity rootEntity) const {
    uint32_t count = 1; // Liczy root entity

    const auto& children = getChildren(rootEntity);
    for (Entity child : children) {
        count += countEntitiesInHierarchy(child); // Rekurencyjnie liczy dzieci
    }

    return count;
}