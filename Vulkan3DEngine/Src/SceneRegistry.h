#pragma once
#include "Entity.h"
#include <string>
#include <vector>
#include <json.hpp>

class EntityManager;
class ComponentManager;
class Serializer;
class SceneManager;

class SceneRegistry {
public:
    explicit SceneRegistry(EntityManager& entityManager, ComponentManager& componentManager,
        Serializer& serializer, SceneManager& sceneManager);

    // Core scene operations
    bool loadScene(const std::string& sceneName);
    bool saveScene(const std::string& sceneName);

    // Scene queries
    const std::string& getCurrentSceneName() const { return m_currentSceneName; }
    bool hasCurrentScene() const { return !m_currentSceneName.empty(); }

private:
    // Module references
    EntityManager& m_entityManager;
    ComponentManager& m_componentManager;
    Serializer& m_serializer;
    SceneManager& m_sceneManager;

    // Current scene tracking
    std::string m_currentSceneName;

    // Helper methods
    nlohmann::json serializeCurrentScene() const;
    bool deserializeScene(const nlohmann::json& sceneData);
    void clearCurrentScene();
};