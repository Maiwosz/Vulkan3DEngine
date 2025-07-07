#pragma once
#include "Entity.h"
#include "Event.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>
#include <string>
#include <json.hpp>
#include <functional>

class ComponentManager; // Forward declaration

struct EntityNode {
    Entity parent{ 0 };
    std::string name;
    std::unordered_set<Entity> children; // For fast lookup
    std::vector<Entity> childrenOrder;   // For maintaining order
    bool locked = false;

    EntityNode() = default;
    EntityNode(const std::string& entityName) : name(entityName) {}
};

class EntityManager {
public:
    explicit EntityManager(ComponentManager& componentManager);

    // Entity lifecycle
    Entity create(const std::string& name = "", Entity parent = Entity(0));
    void destroy(Entity entity);
    bool valid(Entity entity) const;

    // Entity locking system
    void lockEntity(Entity entity);
    void unlockEntity(Entity entity);
    bool isEntityLocked(Entity entity) const;
    bool canModifyEntity(Entity entity) const; // Helper method

    // Entity iteration
    std::set<Entity> getAllEntities() const;

    // Entity naming
    std::string getEntityName(Entity entity) const;
    void setEntityName(Entity entity, const std::string& name);
    Entity findEntityByName(const std::string& name) const;
    bool entityNameExists(const std::string& name) const;

    // Hierarchy operations
    void setParent(Entity child, Entity parent);
    void removeParent(Entity child);
    void addChild(Entity parent, Entity child) { setParent(child, parent); }
    void removeChild(Entity parent, Entity child);

    // Hierarchy queries
    bool hasParent(Entity entity) const;
    Entity getParent(Entity entity) const;
    const std::unordered_set<Entity>& getChildren(Entity entity) const;
    std::vector<Entity> getAllChildren(Entity entity) const;
    std::vector<Entity> getParentChain(Entity entity) const;
    bool isDescendantOf(Entity child, Entity ancestor) const;
    std::vector<Entity> getRootEntities() const;
    int getDepth(Entity entity) const;

    // Order management
    void setEntityOrder(Entity parent, const std::vector<Entity>& childOrder);
    std::vector<Entity> getEntityOrder(Entity parent) const;
    void moveEntityBefore(Entity parent, Entity child, Entity beforeChild);
    void moveEntityAfter(Entity parent, Entity child, Entity afterChild);

    // Global entity order (depth-first traversal)
    const std::vector<Entity>& getGlobalEntityOrder() const;
    void invalidateGlobalOrder();

    // Advanced entity operations
    uint32_t countEntitiesInHierarchy(Entity rootEntity) const;
    Entity cloneEntityHierarchy(Entity sourceEntity, Entity newParent, const std::string& name = "");
    void destroyAllEntities();

    // Entity serialization
    nlohmann::json serializeEntity(Entity entity) const;
    Entity deserializeEntity(const nlohmann::json& entityData, Entity parentEntity = Entity(0));
    nlohmann::json serializeEntityHierarchy(Entity entity) const;
    Entity deserializeEntityHierarchy(const nlohmann::json& hierarchyData, Entity parent = Entity(0));

    // Events
    Event<Entity>& onEntityDestroyed() { return m_onEntityDestroyed; }
    Event<>& onGlobalOrderChanged() { return m_onGlobalOrderChanged; }

private:
    ComponentManager& m_componentManager;

    // Flat entity storage - single source of truth
    std::unordered_map<Entity, EntityNode> m_entityNodes;
    std::set<Entity> m_freeEntities;
    uint32_t m_nextId = 1;

    // Name lookup optimization
    std::unordered_map<std::string, Entity> m_nameToEntity;

    // Root entities order (entities without parent)
    std::vector<Entity> m_rootOrder;

    // Global order cache
    mutable std::vector<Entity> m_globalOrder;
    mutable bool m_globalOrderDirty = true;

    // Events
    Event<Entity> m_onEntityDestroyed;
    Event<> m_onGlobalOrderChanged;

    // Helper methods
    std::string generateUniqueEntityName(const std::string& baseName = "Entity") const;
    void getAllChildrenRecursive(Entity entity, std::vector<Entity>& result) const;
    void removeFromHierarchy(Entity entity);
    void buildGlobalOrder() const;
    void buildGlobalOrderRecursive(Entity entity, std::vector<Entity>& order) const;
    void invalidateOrderAndNotify();

    // Internal node access
    EntityNode* getNode(Entity entity);
    const EntityNode* getNode(Entity entity) const;
};