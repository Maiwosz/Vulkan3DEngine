#pragma once
#include "Entity.h"
#include "Handle.h"
#include <string>
#include <vector>
#include <json.hpp>

class EntityManager;
class ComponentManager;
class SceneManager;
class PrefabInstanceManager;
struct PrefabInstance;

class SceneRegistry {
public:
    explicit SceneRegistry(EntityManager& entityManager, ComponentManager& componentManager,
        PrefabInstanceManager& prefabInstanceManager, SceneManager& sceneManager);

    // Core scene operations
    bool loadScene(const std::string& sceneName);
    bool saveScene(const std::string& sceneName);
    void clearScene();

    // Scene queries
    const std::string& getCurrentSceneName() const { return m_currentSceneName; }
    bool hasCurrentScene() const { return !m_currentSceneName.empty(); }

private:
    // Module references
    EntityManager& m_entityManager;
    ComponentManager& m_componentManager;
    PrefabInstanceManager& m_prefabInstanceManager;
    SceneManager& m_sceneManager;

    // Current scene tracking
    std::string m_currentSceneName;

    // Helper methods
    nlohmann::json serializeCurrentScene() const;
    bool deserializeScene(const nlohmann::json& sceneData);

    // Prefab instance serialization
    nlohmann::json serializePrefabInstance(const PrefabInstance& instance) const;
    bool deserializePrefabInstance(const nlohmann::json& instanceJson);

    // Helper methods for prefab instance handling
    Entity findCorrespondingEntityInInstance(PrefabInstanceHandle instanceHandle, uint32_t originalEntityId) const;
    bool isEntityHierarchyPartOfPrefabInstance(Entity rootEntity) const;
};