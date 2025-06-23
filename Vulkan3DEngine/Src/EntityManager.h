#pragma once
#include "Entity.h"
#include "Event.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>
#include <string>
#include <json.hpp>

class ComponentManager; // Forward declaration

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
    const std::set<Entity>& getAllEntities() const { return m_entities; }

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

private:
    ComponentManager& m_componentManager;

    // Entity storage
    std::set<Entity> m_entities;
    std::set<Entity> m_freeEntities;
    uint32_t m_nextId = 1;

    // Entity locking
    std::unordered_set<Entity> m_lockedEntities;

    // Entity naming
    std::unordered_map<Entity, std::string> m_entityNames;
    std::unordered_map<std::string, Entity> m_nameToEntity;

    // Hierarchy storage
    std::unordered_map<Entity, Entity> m_parentMap;
    std::unordered_map<Entity, std::unordered_set<Entity>> m_childrenMap;
    static const std::unordered_set<Entity> s_emptyChildren;

    // Events
    Event<Entity> m_onEntityDestroyed;

    // Helper methods
    std::string generateUniqueEntityName(const std::string& baseName = "Entity") const;
    void getAllChildrenRecursive(Entity entity, std::vector<Entity>& result) const;
    void removeFromHierarchy(Entity entity);
};