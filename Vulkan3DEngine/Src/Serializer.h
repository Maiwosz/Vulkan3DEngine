#pragma once
#include <vector>
#include <json.hpp>
#include "Entity.h"

class Registry;

class Serializer {
public:
    explicit Serializer(Registry& registry);

    // Entity serialization
    nlohmann::json serializeEntity(Entity entity) const;
    Entity deserializeEntity(const nlohmann::json& entityData, Entity parentEntity = Entity(0));

    // Component serialization
    nlohmann::json serializeComponent(Entity entity, const std::string& componentTypeName) const;
    void deserializeComponent(Entity entity, const std::string& componentTypeName, const nlohmann::json& componentData);

    // Helper methods for prefab operations
    nlohmann::json serializeEntityHierarchy(Entity entity) const;
    Entity deserializePrefabHierarchy(const nlohmann::json& hierarchyData, Entity parent);

private:
    Registry& m_registry;
};