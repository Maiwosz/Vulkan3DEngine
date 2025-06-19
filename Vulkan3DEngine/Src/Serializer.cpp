#include "Serializer.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include <stdexcept>
#include <spdlog/spdlog.h>

Serializer::Serializer(EntityManager& entityManager, ComponentManager& componentManager)
    : m_entityManager(entityManager), m_componentManager(componentManager) {
}

// Entity serialization
nlohmann::json Serializer::serializeEntity(Entity entity) const {
    if (!m_entityManager.valid(entity)) {
        throw std::runtime_error("Invalid entity for serialization");
    }

    nlohmann::json result;
    result["id"] = entity.id;
    result["name"] = m_entityManager.getEntityName(entity);
    result["parent"] = m_entityManager.hasParent(entity) ? m_entityManager.getParent(entity).id : 0;

    nlohmann::json componentsJson = nlohmann::json::array();
    auto componentTypes = m_componentManager.getEntityComponentTypes(entity);

    for (const std::string& typeName : componentTypes) {
        nlohmann::json componentJson;
        componentJson["type"] = typeName;
        componentJson["data"] = serializeComponent(entity, typeName);
        componentsJson.push_back(componentJson);
    }
    result["components"] = componentsJson;

    const auto& children = m_entityManager.getChildren(entity);
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
    Entity newEntity = m_entityManager.create(entityName);

    // Set parent if provided
    if (parentEntity.id != 0 && m_entityManager.valid(parentEntity)) {
        m_entityManager.setParent(newEntity, parentEntity);
    }

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
    return m_componentManager.serializeComponent(entity, componentTypeName);
}

void Serializer::deserializeComponent(Entity entity, const std::string& componentTypeName, const nlohmann::json& componentData) {
    m_componentManager.deserializeComponent(entity, componentTypeName, componentData);
}

nlohmann::json Serializer::serializeEntityHierarchy(Entity entity) const {
    nlohmann::json result;
    result["entity"] = serializeEntity(entity);

    const auto& children = m_entityManager.getChildren(entity);
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
    Entity newEntity = deserializeEntity(hierarchyData["entity"], parent);

    if (hierarchyData.contains("children")) {
        const auto& childrenArray = hierarchyData["children"];
        for (const auto& childData : childrenArray) {
            deserializePrefabHierarchy(childData, newEntity);
        }
    }

    return newEntity;
}