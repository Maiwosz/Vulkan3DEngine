#include "EntityManager.h"
#include "ComponentManager.h"
#include <algorithm>
#include <spdlog/spdlog.h>

const std::unordered_set<Entity> EntityManager::s_emptyChildren;

EntityManager::EntityManager(ComponentManager& componentManager)
    : m_componentManager(componentManager) {
}

// Entity lifecycle
Entity EntityManager::create(const std::string& name) {
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

    // Set entity name
    std::string finalName = name.empty() ? generateUniqueEntityName() : name;
    if (entityNameExists(finalName)) {
        finalName = generateUniqueEntityName(finalName);
    }

    m_entityNames[entity] = finalName;
    m_nameToEntity[finalName] = entity;

    return entity;
}

void EntityManager::destroy(Entity entity) {
    if (!valid(entity)) return;

    // Sprawdź czy entity jest zablokowane
    if (isEntityLocked(entity)) {
        SPDLOG_WARN("Cannot destroy locked entity: {}", entity.id);
        return;
    }

    // Wyślij event PRZED usunięciem
    m_onEntityDestroyed.invoke(entity);

	if (!valid(entity)) return; // Entity mogło być usunięte przez event

    // Remove name mappings
    auto nameIt = m_entityNames.find(entity);
    if (nameIt != m_entityNames.end()) {
        m_nameToEntity.erase(nameIt->second);
        m_entityNames.erase(nameIt);
    }

    // Remove from hierarchy and reparent children
    removeFromHierarchy(entity);

    // Remove all components
    m_componentManager.removeAllComponents(entity);

    // Remove from locked entities
    m_lockedEntities.erase(entity);

    // Free entity
    m_entities.erase(entity);
    m_freeEntities.insert(entity);
}

bool EntityManager::valid(Entity entity) const {
    return m_entities.count(entity) > 0;
}

// Entity locking system
void EntityManager::lockEntity(Entity entity) {
    if (valid(entity)) {
        m_lockedEntities.insert(entity);
    }
}

void EntityManager::unlockEntity(Entity entity) {
    m_lockedEntities.erase(entity);
}

bool EntityManager::isEntityLocked(Entity entity) const {
    return m_lockedEntities.count(entity) > 0;
}

bool EntityManager::canModifyEntity(Entity entity) const {
    return valid(entity) && !isEntityLocked(entity);
}

// Entity naming
std::string EntityManager::getEntityName(Entity entity) const {
    auto it = m_entityNames.find(entity);
    return (it != m_entityNames.end()) ? it->second : "";
}

void EntityManager::setEntityName(Entity entity, const std::string& name) {
    if (!canModifyEntity(entity)) return;

    // Remove old mapping
    auto oldNameIt = m_entityNames.find(entity);
    if (oldNameIt != m_entityNames.end()) {
        m_nameToEntity.erase(oldNameIt->second);
    }

    // Generate unique name if needed
    std::string finalName = name;
    if (entityNameExists(finalName)) {
        finalName = generateUniqueEntityName(finalName);
    }

    // Set new mapping
    m_entityNames[entity] = finalName;
    m_nameToEntity[finalName] = entity;
}

Entity EntityManager::findEntityByName(const std::string& name) const {
    auto it = m_nameToEntity.find(name);
    return (it != m_nameToEntity.end()) ? it->second : Entity(0);
}

bool EntityManager::entityNameExists(const std::string& name) const {
    return m_nameToEntity.find(name) != m_nameToEntity.end();
}

// Hierarchy operations
void EntityManager::setParent(Entity child, Entity parent) {
    if (!canModifyEntity(child) || !valid(parent)) return;

    // Prevent cycles
    if (isDescendantOf(parent, child)) {
        return;
    }

    // Remove previous parent if exists
    if (hasParent(child)) {
        removeParent(child);
    }

    // Set new parent
    m_parentMap[child] = parent;
    m_childrenMap[parent].insert(child);
}

void EntityManager::removeParent(Entity child) {
    if (!canModifyEntity(child)) return;

    auto it = m_parentMap.find(child);
    if (it != m_parentMap.end()) {
        Entity parent = it->second;
        m_parentMap.erase(it);

        // Remove from parent's children list
        if (auto childrenIt = m_childrenMap.find(parent); childrenIt != m_childrenMap.end()) {
            childrenIt->second.erase(child);
            if (childrenIt->second.empty()) {
                m_childrenMap.erase(childrenIt);
            }
        }
    }
}

void EntityManager::removeChild(Entity parent, Entity child) {
    auto it = m_parentMap.find(child);
    if (it != m_parentMap.end() && it->second == parent) {
        removeParent(child);
    }
}

// Hierarchy queries
bool EntityManager::hasParent(Entity entity) const {
    return m_parentMap.count(entity) > 0;
}

Entity EntityManager::getParent(Entity entity) const {
    auto it = m_parentMap.find(entity);
    return (it != m_parentMap.end()) ? it->second : Entity(0);
}

const std::unordered_set<Entity>& EntityManager::getChildren(Entity entity) const {
    auto it = m_childrenMap.find(entity);
    return (it != m_childrenMap.end()) ? it->second : s_emptyChildren;
}

std::vector<Entity> EntityManager::getAllChildren(Entity entity) const {
    std::vector<Entity> result;
    getAllChildrenRecursive(entity, result);
    return result;
}

std::vector<Entity> EntityManager::getParentChain(Entity entity) const {
    std::vector<Entity> chain;
    Entity current = entity;

    while (hasParent(current)) {
        current = getParent(current);
        chain.push_back(current);
    }

    return chain;
}

bool EntityManager::isDescendantOf(Entity child, Entity ancestor) const {
    Entity current = child;

    while (hasParent(current)) {
        current = getParent(current);
        if (current == ancestor) {
            return true;
        }
    }

    return false;
}

std::vector<Entity> EntityManager::getRootEntities() const {
    std::vector<Entity> roots;

    // Iterate through ALL entities, not just parents
    for (Entity entity : m_entities) {
        if (!hasParent(entity)) {
            roots.push_back(entity);
        }
    }

    return roots;
}

int EntityManager::getDepth(Entity entity) const {
    int depth = 0;
    Entity current = entity;

    while (hasParent(current)) {
        current = getParent(current);
        depth++;
    }

    return depth;
}

// Advanced entity operations
Entity EntityManager::createChild(Entity parent, const std::string& name) {
    Entity child = create(name);

    if (parent.id != 0 && valid(parent)) {
        setParent(child, parent);
    }

    return child;
}

uint32_t EntityManager::countEntitiesInHierarchy(Entity rootEntity) const {
    if (!valid(rootEntity)) {
        return 0;
    }

    uint32_t count = 1; // Count root entity

    const auto& children = getChildren(rootEntity);
    for (Entity child : children) {
        count += countEntitiesInHierarchy(child); // Recursively count children
    }

    return count;
}

Entity EntityManager::cloneEntityHierarchy(Entity sourceEntity, Entity newParent, const std::string& name) {
    if (!valid(sourceEntity)) {
        return Entity(0);
    }

    // Determine final name
    std::string finalName = name;
    if (finalName.empty()) {
        std::string sourceName = getEntityName(sourceEntity);
        finalName = sourceName.empty() ? "Entity_Copy" : sourceName + "_Copy";
    }

    // Create new entity
    Entity newEntity = create(finalName);

    // Set parent if provided
    if (newParent.id != 0 && valid(newParent)) {
        setParent(newEntity, newParent);
    }

    // Copy all components using serialization
    auto componentTypes = m_componentManager.getEntityComponentTypes(sourceEntity);
    for (const std::string& typeName : componentTypes) {
        auto componentData = m_componentManager.serializeComponent(sourceEntity, typeName);
        m_componentManager.deserializeComponent(newEntity, typeName, componentData);
    }

    // Recursively clone children
    const auto& children = getChildren(sourceEntity);
    for (Entity child : children) {
        cloneEntityHierarchy(child, newEntity); // Recursive clone
    }

    return newEntity;
}

void EntityManager::destroyAllEntities()
{
    auto allEntities = getAllEntities();
    std::vector<Entity> entitiesToDestroy(allEntities.begin(), allEntities.end());

    for (Entity entity : entitiesToDestroy) {
        if (valid(entity)) {
            destroy(entity);
        }
    }
}

// Entity serialization
nlohmann::json EntityManager::serializeEntity(Entity entity) const {
    if (!valid(entity)) {
        throw std::runtime_error("Invalid entity for serialization");
    }

    nlohmann::json result;
    result["id"] = entity.id;
    result["name"] = getEntityName(entity);
    result["parent"] = hasParent(entity) ? getParent(entity).id : 0;

    nlohmann::json componentsJson = nlohmann::json::array();
    auto componentTypes = m_componentManager.getEntityComponentTypes(entity);

    for (const std::string& typeName : componentTypes) {
        nlohmann::json componentJson;
        componentJson["type"] = typeName;
        componentJson["data"] = m_componentManager.serializeComponent(entity, typeName);
        componentsJson.push_back(componentJson);
    }
    result["components"] = componentsJson;

    const auto& children = getChildren(entity);
    if (!children.empty()) {
        nlohmann::json childrenJson = nlohmann::json::array();
        for (Entity child : children) {
            childrenJson.push_back(serializeEntity(child));
        }
        result["children"] = childrenJson;
    }

    return result;
}

Entity EntityManager::deserializeEntity(const nlohmann::json& entityData, Entity parentEntity) {
    std::string entityName = entityData.contains("name") ? entityData["name"].get<std::string>() : "";
    Entity newEntity = create(entityName);

    // Set parent if provided
    if (parentEntity.id != 0 && valid(parentEntity)) {
        setParent(newEntity, parentEntity);
    }

    if (entityData.contains("components")) {
        for (const auto& componentJson : entityData["components"]) {
            std::string typeName = componentJson["type"];
            m_componentManager.deserializeComponent(newEntity, typeName, componentJson["data"]);
        }
    }

    if (entityData.contains("children")) {
        for (const auto& childJson : entityData["children"]) {
            deserializeEntity(childJson, newEntity);
        }
    }

    return newEntity;
}

nlohmann::json EntityManager::serializeEntityHierarchy(Entity entity) const {
    nlohmann::json result;
    result["entity"] = serializeEntity(entity);

    const auto& children = getChildren(entity);
    if (!children.empty()) {
        nlohmann::json childrenArray = nlohmann::json::array();
        for (Entity child : children) {
            childrenArray.push_back(serializeEntityHierarchy(child));
        }
        result["children"] = std::move(childrenArray);
    }

    return result;
}

Entity EntityManager::deserializeEntityHierarchy(const nlohmann::json& hierarchyData, Entity parent) {
    Entity newEntity = deserializeEntity(hierarchyData["entity"], parent);

    if (hierarchyData.contains("children")) {
        const auto& childrenArray = hierarchyData["children"];
        for (const auto& childData : childrenArray) {
            deserializeEntityHierarchy(childData, newEntity);
        }
    }

    return newEntity;
}

// Helper methods
std::string EntityManager::generateUniqueEntityName(const std::string& baseName) const {
    std::string base = baseName.empty() ? "Entity" : baseName;
    std::string candidate = base;

    int counter = 1;
    while (entityNameExists(candidate)) {
        candidate = base + "_" + std::to_string(counter);
        counter++;
    }

    return candidate;
}

void EntityManager::getAllChildrenRecursive(Entity entity, std::vector<Entity>& result) const {
    const auto& children = getChildren(entity);
    for (Entity child : children) {
        result.push_back(child);
        getAllChildrenRecursive(child, result);
    }
}

void EntityManager::removeFromHierarchy(Entity entity) {
    // Make a COPY of children before removing entity to avoid iterator invalidation
    std::vector<Entity> childrenCopy;
    const auto& children = getChildren(entity);
    childrenCopy.reserve(children.size());
    for (Entity child : children) {
        childrenCopy.push_back(child);
    }

    Entity parent = hasParent(entity) ? getParent(entity) : Entity(0);

    // Reparent children to entity's parent (or make them roots)
    for (Entity child : childrenCopy) {  // Now safe to iterate
        removeParent(child);
        if (parent.id != 0) {
            setParent(child, parent);
        }
    }

    // Remove entity from hierarchy
    removeParent(entity);
    m_childrenMap.erase(entity);
}