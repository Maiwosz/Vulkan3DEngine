#pragma once
#include <vector>
#include <json.hpp>
#include "Entity.h"

class ComponentManager;
class EntityManager;

class Serializer {
public:
    Serializer(EntityManager& entityManager, ComponentManager& componentManager);

    // Entity serialization
    nlohmann::json serializeEntity(Entity entity) const;
    Entity deserializeEntity(const nlohmann::json& entityData, Entity parentEntity = Entity(0));

    // Component serialization
    nlohmann::json serializeComponent(Entity entity, const std::string& componentTypeName) const;
    void deserializeComponent(Entity entity, const std::string& componentTypeName, const nlohmann::json& componentData);

    // Helper methods for prefab operations
    nlohmann::json serializeEntityHierarchy(Entity entity) const;
    Entity deserializePrefabHierarchy(const nlohmann::json& hierarchyData, Entity parent = Entity(0));

private:
    EntityManager& m_entityManager;
    ComponentManager& m_componentManager;
};