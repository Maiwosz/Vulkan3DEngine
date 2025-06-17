#include "Serializer.h"
#include "Registry.h"
#include <stdexcept>
#include <spdlog/spdlog.h>

Serializer::Serializer(Registry& registry) : m_registry(registry) {
}

// Entity serialization
nlohmann::json Serializer::serializeEntity(Entity entity) const {
    if (!m_registry.valid(entity)) {
        throw std::runtime_error("Invalid entity for serialization");
    }

    nlohmann::json result;
    result["id"] = entity.id;
    result["name"] = m_registry.getEntityName(entity);
    result["parent"] = m_registry.hasParent(entity) ? m_registry.getParent(entity).id : 0;

    nlohmann::json componentsJson = nlohmann::json::array();
    auto componentTypes = m_registry.getEntityComponentTypes(entity);

    for (const std::string& typeName : componentTypes) {
        nlohmann::json componentJson;
        componentJson["type"] = typeName;
        componentJson["data"] = serializeComponent(entity, typeName);
        componentsJson.push_back(componentJson);
    }
    result["components"] = componentsJson;

    const auto& children = m_registry.getChildren(entity);
    if (!children.empty()) {
        nlohmann::json childrenJson = nlohmann::json::array();
        for (Entity child : children) {
            childrenJson.push_back(serializeEntity(child));
        }
        result["children"] = childrenJson;
    }

    return result;
}

Entity Serializer::deserializeEntity(const nlohmann::json& entityData, Entity parentEntity) {
    std::string entityName = entityData.contains("name") ? entityData["name"].get<std::string>() : "";
    Entity newEntity = m_registry.create(parentEntity, entityName);

    if (entityData.contains("components")) {
        for (const auto& componentJson : entityData["components"]) {
            std::string typeName = componentJson["type"];
            deserializeComponent(newEntity, typeName, componentJson["data"]);
        }
    }

    if (entityData.contains("children")) {
        for (const auto& childJson : entityData["children"]) {
            deserializeEntity(childJson, newEntity);
        }
    }

    return newEntity;
}

// Component serialization
nlohmann::json Serializer::serializeComponent(Entity entity, const std::string& componentTypeName) const {
    auto* pool = m_registry.getComponentPool(componentTypeName);
    if (!pool) {
        throw std::runtime_error("Component type not found: " + componentTypeName);
    }
    return pool->serializeComponent(entity);
}

void Serializer::deserializeComponent(Entity entity, const std::string& componentTypeName, const nlohmann::json& componentData) {
    auto* pool = m_registry.getComponentPool(componentTypeName);
    if (!pool) {
        throw std::runtime_error("Component type not found: " + componentTypeName);
    }
    pool->deserializeComponent(entity, componentData, m_registry);
}

nlohmann::json Serializer::serializeEntityHierarchy(Entity entity) const {
    nlohmann::json result;

    // Serializuj podstawowe dane entity
    result["entity"] = serializeEntity(entity);

    // Serializuj dzieci rekurencyjnie
    const auto& children = m_registry.getChildren(entity);
    if (!children.empty()) {
        nlohmann::json childrenArray = nlohmann::json::array();
        for (Entity child : children) {
            childrenArray.push_back(serializeEntityHierarchy(child));
        }
        result["children"] = std::move(childrenArray);
    }

    return result;
}

Entity Serializer::deserializePrefabHierarchy(const nlohmann::json& hierarchyData, Entity parent) {
    // Deserializuj główne entity
    Entity newEntity = deserializeEntity(hierarchyData["entity"], parent);

    // Deserializuj dzieci jeśli istnieją
    if (hierarchyData.contains("children")) {
        const auto& childrenArray = hierarchyData["children"];
        for (const auto& childData : childrenArray) {
            deserializePrefabHierarchy(childData, newEntity);
        }
    }

    return newEntity;
}
