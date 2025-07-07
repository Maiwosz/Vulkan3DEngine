#include "EntityManager.h"
#include "ComponentManager.h"
#include <algorithm>
#include <spdlog/spdlog.h>

EntityManager::EntityManager(ComponentManager& componentManager)
    : m_componentManager(componentManager) {
    // Setup entity order callback for ComponentManager
    m_componentManager.setEntityOrderCallback([this]() -> const std::vector<Entity>&{
        return this->getGlobalEntityOrder();
        });
}

// Internal node access
EntityNode* EntityManager::getNode(Entity entity) {
    auto it = m_entityNodes.find(entity);
    return (it != m_entityNodes.end()) ? &it->second : nullptr;
}

const EntityNode* EntityManager::getNode(Entity entity) const {
    auto it = m_entityNodes.find(entity);
    return (it != m_entityNodes.end()) ? &it->second : nullptr;
}

// Entity lifecycle
Entity EntityManager::create(const std::string& name, Entity parent) {
    Entity entity;

    if (!m_freeEntities.empty()) {
        auto it = m_freeEntities.begin();
        entity = *it;
        m_freeEntities.erase(it);
    }
    else {
        entity = Entity(m_nextId++);
    }

    // Set entity name
    std::string finalName = name.empty() ? generateUniqueEntityName() : name;
    if (entityNameExists(finalName)) {
        finalName = generateUniqueEntityName(finalName);
    }

    // Create node
    EntityNode node(finalName);
    m_entityNodes[entity] = std::move(node);
    m_nameToEntity[finalName] = entity;

    // Set parent if provided and valid
    if (parent.id != 0 && valid(parent)) {
        setParent(entity, parent);
    }
    else {
        // Add to root order
        m_rootOrder.push_back(entity);
        invalidateOrderAndNotify();
    }

    return entity;
}

void EntityManager::destroy(Entity entity) {
    if (!valid(entity)) return;

    // Check if entity is locked
    if (isEntityLocked(entity)) {
        SPDLOG_WARN("Cannot destroy locked entity: {}", entity.id);
        return;
    }

    // Send event BEFORE removal
    m_onEntityDestroyed.invoke(entity);

    if (!valid(entity)) return; // Entity might have been deleted by event

    EntityNode* node = getNode(entity);
    if (!node) return;

    // Remove name mapping
    m_nameToEntity.erase(node->name);

    // Remove from hierarchy and reparent children
    removeFromHierarchy(entity);

    // Remove from root order if it's a root entity
    if (node->parent.id == 0) {
        auto it = std::find(m_rootOrder.begin(), m_rootOrder.end(), entity);
        if (it != m_rootOrder.end()) {
            m_rootOrder.erase(it);
        }
    }

    // Remove all components
    m_componentManager.removeAllComponents(entity);

    // Remove node
    m_entityNodes.erase(entity);

    // Free entity
    m_freeEntities.insert(entity);

    invalidateOrderAndNotify();
}

bool EntityManager::valid(Entity entity) const {
    return m_entityNodes.find(entity) != m_entityNodes.end();
}

std::set<Entity> EntityManager::getAllEntities() const {
    std::set<Entity> entities;
    for (const auto& [entity, node] : m_entityNodes) {
        entities.insert(entity);
    }
    return entities;
}

// Entity locking system
void EntityManager::lockEntity(Entity entity) {
    if (EntityNode* node = getNode(entity)) {
        node->locked = true;
    }
}

void EntityManager::unlockEntity(Entity entity) {
    if (EntityNode* node = getNode(entity)) {
        node->locked = false;
    }
}

bool EntityManager::isEntityLocked(Entity entity) const {
    if (const EntityNode* node = getNode(entity)) {
        return node->locked;
    }
    return false;
}

bool EntityManager::canModifyEntity(Entity entity) const {
    return valid(entity) && !isEntityLocked(entity);
}

// Entity naming
std::string EntityManager::getEntityName(Entity entity) const {
    if (const EntityNode* node = getNode(entity)) {
        return node->name;
    }
    return "";
}

void EntityManager::setEntityName(Entity entity, const std::string& name) {
    if (!canModifyEntity(entity)) return;

    EntityNode* node = getNode(entity);
    if (!node) return;

    // Remove old mapping
    m_nameToEntity.erase(node->name);

    // Generate unique name if needed
    std::string finalName = name;
    if (entityNameExists(finalName)) {
        finalName = generateUniqueEntityName(finalName);
    }

    // Set new mapping
    node->name = finalName;
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

    EntityNode* childNode = getNode(child);
    EntityNode* parentNode = getNode(parent);
    if (!childNode || !parentNode) return;

    // Prevent cycles
    if (isDescendantOf(parent, child)) {
        return;
    }

    // Remove from previous parent
    if (childNode->parent.id != 0) {
        if (EntityNode* oldParentNode = getNode(childNode->parent)) {
            oldParentNode->children.erase(child);
            // Remove from order as well
            auto it = std::find(oldParentNode->childrenOrder.begin(), oldParentNode->childrenOrder.end(), child);
            if (it != oldParentNode->childrenOrder.end()) {
                oldParentNode->childrenOrder.erase(it);
            }
        }
    }
    else {
        // Remove from root order if it was a root entity
        auto it = std::find(m_rootOrder.begin(), m_rootOrder.end(), child);
        if (it != m_rootOrder.end()) {
            m_rootOrder.erase(it);
        }
    }

    // Set new parent
    childNode->parent = parent;
    parentNode->children.insert(child);
    parentNode->childrenOrder.push_back(child); // Add to end of order

    invalidateOrderAndNotify();
}

void EntityManager::removeParent(Entity child) {
    if (!canModifyEntity(child)) return;

    EntityNode* childNode = getNode(child);
    if (!childNode || childNode->parent.id == 0) return;

    // Remove from parent's children
    if (EntityNode* parentNode = getNode(childNode->parent)) {
        parentNode->children.erase(child);
        // Remove from order as well
        auto it = std::find(parentNode->childrenOrder.begin(), parentNode->childrenOrder.end(), child);
        if (it != parentNode->childrenOrder.end()) {
            parentNode->childrenOrder.erase(it);
        }
    }

    // Clear parent and add to root order
    childNode->parent = Entity(0);
    m_rootOrder.push_back(child);

    invalidateOrderAndNotify();
}

void EntityManager::removeChild(Entity parent, Entity child) {
    if (const EntityNode* childNode = getNode(child)) {
        if (childNode->parent == parent) {
            removeParent(child);
        }
    }
}

// Hierarchy queries
bool EntityManager::hasParent(Entity entity) const {
    if (const EntityNode* node = getNode(entity)) {
        return node->parent.id != 0;
    }
    return false;
}

Entity EntityManager::getParent(Entity entity) const {
    if (const EntityNode* node = getNode(entity)) {
        return node->parent;
    }
    return Entity(0);
}

const std::unordered_set<Entity>& EntityManager::getChildren(Entity entity) const {
    static const std::unordered_set<Entity> emptyChildren;
    if (const EntityNode* node = getNode(entity)) {
        return node->children;
    }
    return emptyChildren;
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

    for (const auto& [entity, node] : m_entityNodes) {
        if (node.parent.id == 0) {
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

void EntityManager::setEntityOrder(Entity parent, const std::vector<Entity>& childOrder) {
    // Jeśli parent ma ID 0, to operujemy na root entities
    if (parent.id == 0) {
        // Validate that all entities in childOrder are actually root entities
        for (Entity entity : childOrder) {
            if (!valid(entity) || hasParent(entity)) {
                return; // Invalid root entity found
            }
        }

        // Get current root entities
        auto currentRoots = getRootEntities();

        // Ensure all root entities are included in the order
        if (childOrder.size() != currentRoots.size()) {
            return; // Missing root entities
        }

        m_rootOrder = childOrder;
        invalidateOrderAndNotify();
    }
    else {
        // Operujemy na children danego parent
        if (!canModifyEntity(parent)) return;

        EntityNode* parentNode = getNode(parent);
        if (!parentNode) return;

        // Validate that all entities in childOrder are actually children of parent
        for (Entity child : childOrder) {
            if (parentNode->children.find(child) == parentNode->children.end()) {
                return; // Invalid child found
            }
        }

        // Ensure all children are included in the order
        if (childOrder.size() != parentNode->children.size()) {
            return; // Missing children
        }

        parentNode->childrenOrder = childOrder;
        invalidateOrderAndNotify();
    }
}

std::vector<Entity> EntityManager::getEntityOrder(Entity parent) const {
    // Jeśli parent ma ID 0, zwracamy kolejność root entities
    if (parent.id == 0) {
        return m_rootOrder;
    }
    else {
        // Zwracamy kolejność children danego parent
        if (const EntityNode* parentNode = getNode(parent)) {
            return parentNode->childrenOrder;
        }
        return {};
    }
}

void EntityManager::moveEntityBefore(Entity parent, Entity child, Entity beforeChild) {
    if (parent.id == 0) {
        // Operujemy na root entities
        auto& order = m_rootOrder;

        // Find positions
        auto childIt = std::find(order.begin(), order.end(), child);
        auto beforeIt = std::find(order.begin(), order.end(), beforeChild);

        if (childIt == order.end() || beforeIt == order.end()) return;

        // Remove child from current position
        order.erase(childIt);

        // Find new position for beforeChild (might have changed after erase)
        beforeIt = std::find(order.begin(), order.end(), beforeChild);

        // Insert before
        order.insert(beforeIt, child);

        invalidateOrderAndNotify();
    }
    else {
        // Operujemy na children danego parent
        if (!canModifyEntity(parent)) return;

        EntityNode* parentNode = getNode(parent);
        if (!parentNode) return;

        auto& order = parentNode->childrenOrder;

        // Find positions
        auto childIt = std::find(order.begin(), order.end(), child);
        auto beforeIt = std::find(order.begin(), order.end(), beforeChild);

        if (childIt == order.end() || beforeIt == order.end()) return;

        // Remove child from current position
        order.erase(childIt);

        // Find new position for beforeChild (might have changed after erase)
        beforeIt = std::find(order.begin(), order.end(), beforeChild);

        // Insert before
        order.insert(beforeIt, child);

        invalidateOrderAndNotify();
    }
}

void EntityManager::moveEntityAfter(Entity parent, Entity child, Entity afterChild) {
    if (parent.id == 0) {
        // Operujemy na root entities
        auto& order = m_rootOrder;

        // Find positions
        auto childIt = std::find(order.begin(), order.end(), child);
        auto afterIt = std::find(order.begin(), order.end(), afterChild);

        if (childIt == order.end() || afterIt == order.end()) return;

        // Remove child from current position
        order.erase(childIt);

        // Find new position for afterChild (might have changed after erase)
        afterIt = std::find(order.begin(), order.end(), afterChild);

        // Insert after
        order.insert(afterIt + 1, child);

        invalidateOrderAndNotify();
    }
    else {
        // Operujemy na children danego parent
        if (!canModifyEntity(parent)) return;

        EntityNode* parentNode = getNode(parent);
        if (!parentNode) return;

        auto& order = parentNode->childrenOrder;

        // Find positions
        auto childIt = std::find(order.begin(), order.end(), child);
        auto afterIt = std::find(order.begin(), order.end(), afterChild);

        if (childIt == order.end() || afterIt == order.end()) return;

        // Remove child from current position
        order.erase(childIt);

        // Find new position for afterChild (might have changed after erase)
        afterIt = std::find(order.begin(), order.end(), afterChild);

        // Insert after
        order.insert(afterIt + 1, child);

        invalidateOrderAndNotify();
    }
}

const std::vector<Entity>& EntityManager::getGlobalEntityOrder() const {
    if (m_globalOrderDirty) {
        buildGlobalOrder();
    }
    return m_globalOrder;
}

void EntityManager::invalidateGlobalOrder() {
    m_globalOrderDirty = true;
}

// Advanced entity operations
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

void EntityManager::destroyAllEntities() {
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
    result["name"] = getEntityName(entity);

    // Serialize components
    nlohmann::json componentsJson = nlohmann::json::array();
    auto componentTypes = m_componentManager.getEntityComponentTypes(entity);

    for (const std::string& typeName : componentTypes) {
        nlohmann::json componentJson;
        componentJson["type"] = typeName;
        componentJson["data"] = m_componentManager.serializeComponent(entity, typeName);
        componentsJson.push_back(componentJson);
    }

    if (!componentsJson.empty()) {
        result["components"] = componentsJson;
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

    // Deserialize components
    if (entityData.contains("components")) {
        for (const auto& componentJson : entityData["components"]) {
            std::string typeName = componentJson["type"];
            m_componentManager.deserializeComponent(newEntity, typeName, componentJson["data"]);
        }
    }

    return newEntity;
}

nlohmann::json EntityManager::serializeEntityHierarchy(Entity entity) const {
    if (!valid(entity)) {
        throw std::runtime_error("Invalid entity for hierarchy serialization");
    }

    nlohmann::json result;

    // Use serializeEntity for basic data
    result["entity"] = serializeEntity(entity);

    // Add children if they exist
    const auto& children = getChildren(entity);
    if (!children.empty()) {
        nlohmann::json childrenArray = nlohmann::json::array();

        // Sort children by ID for predictable order
        std::vector<Entity> sortedChildren(children.begin(), children.end());
        std::sort(sortedChildren.begin(), sortedChildren.end(),
            [](const Entity& a, const Entity& b) { return a.id < b.id; });

        for (Entity child : sortedChildren) {
            childrenArray.push_back(serializeEntityHierarchy(child));
        }
        result["children"] = std::move(childrenArray);
    }

    return result;
}

Entity EntityManager::deserializeEntityHierarchy(const nlohmann::json& hierarchyData, Entity parent) {
    if (!hierarchyData.contains("entity")) {
        throw std::runtime_error("Invalid hierarchy data: missing 'entity' field");
    }

    // Create main entity
    Entity newEntity = deserializeEntity(hierarchyData["entity"], parent);

    // Deserialize children if they exist
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
    EntityNode* node = getNode(entity);
    if (!node) return;

    // Make a copy of children to avoid iterator invalidation
    std::vector<Entity> childrenCopy(node->children.begin(), node->children.end());

    // Reparent children to entity's parent (or make them roots)
    for (Entity child : childrenCopy) {
        if (EntityNode* childNode = getNode(child)) {
            childNode->parent = node->parent;

            // Add to new parent's children list
            if (node->parent.id != 0) {
                if (EntityNode* parentNode = getNode(node->parent)) {
                    parentNode->children.insert(child);
                }
            }
        }
    }

    // Remove entity from its parent's children list
    if (node->parent.id != 0) {
        if (EntityNode* parentNode = getNode(node->parent)) {
            parentNode->children.erase(entity);
        }
    }

    // Clear node's relationships
    node->parent = Entity(0);
    node->children.clear();
}

void EntityManager::buildGlobalOrder() const {
    m_globalOrder.clear();

    // Process root entities in order
    for (Entity rootEntity : m_rootOrder) {
        if (valid(rootEntity)) {
            buildGlobalOrderRecursive(rootEntity, m_globalOrder);
        }
    }

    m_globalOrderDirty = false;
}

void EntityManager::buildGlobalOrderRecursive(Entity entity, std::vector<Entity>& order) const {
    order.push_back(entity);

    if (const EntityNode* node = getNode(entity)) {
        // Process children in order
        for (Entity child : node->childrenOrder) {
            if (valid(child)) {
                buildGlobalOrderRecursive(child, order);
            }
        }
    }
}

void EntityManager::invalidateOrderAndNotify() {
    invalidateGlobalOrder();
    m_componentManager.invalidateAllOrders();
    m_onGlobalOrderChanged.invoke();
}
