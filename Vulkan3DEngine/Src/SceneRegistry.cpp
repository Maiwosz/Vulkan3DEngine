#include "SceneRegistry.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include "SceneManager.h"
#include <spdlog/spdlog.h>
#include "PrefabInstanceManager.h"

SceneRegistry::SceneRegistry(EntityManager& entityManager, ComponentManager& componentManager,
    PrefabInstanceManager& prefabInstanceManager, SceneManager& sceneManager)
    : m_entityManager(entityManager)
    , m_componentManager(componentManager)
    , m_prefabInstanceManager(prefabInstanceManager)
    , m_sceneManager(sceneManager) {
}

bool SceneRegistry::loadScene(const std::string& sceneName) {
    // Get scene data from SceneManager
    nlohmann::json sceneData = m_sceneManager.getSceneData(sceneName);
    if (sceneData.empty()) {
        SPDLOG_ERROR("Failed to get scene data for {}", sceneName);
        return false;
    }

    // Deserialize new scene
    if (!deserializeScene(sceneData)) {
        SPDLOG_ERROR("Failed to deserialize scene {}", sceneName);
        return false;
    }

    // Update current scene name
    m_currentSceneName = sceneName;

    SPDLOG_INFO("Successfully loaded scene {}", sceneName);
    return true;
}

bool SceneRegistry::saveScene(const std::string& sceneName) {
    // Serialize current scene
    nlohmann::json sceneData = serializeCurrentScene();

    // Save through SceneManager
    if (!m_sceneManager.saveSceneToFile(sceneName, sceneData)) {
        SPDLOG_ERROR("Failed to save scene {}", sceneName);
        return false;
    }

    // Update current scene name
    m_currentSceneName = sceneName;

    SPDLOG_INFO("Successfully saved scene {}", sceneName);
    return true;
}

void SceneRegistry::clearScene()
{
    // Najpierw zniszcz wszystkie instancje prefabów
    m_prefabInstanceManager.destroyAllPrefabInstances();

    // Następnie zniszcz pozostałe entity
    m_entityManager.destroyAllEntities();
}

nlohmann::json SceneRegistry::serializeCurrentScene() const {
    nlohmann::json sceneData;
    nlohmann::json& entitiesJson = sceneData["entities"];

    // Get all root entities (entities without parents)
    std::vector<Entity> rootEntities = m_entityManager.getRootEntities();

    SPDLOG_INFO("Serializing scene with {} root entities", rootEntities.size());

    // Serialize each root entity hierarchy using the hierarchy format
    for (Entity rootEntity : rootEntities) {
        SPDLOG_DEBUG("Serializing root entity {} ({})", rootEntity.id, m_entityManager.getEntityName(rootEntity));

        // Count entities in this hierarchy for debugging
        uint32_t hierarchyCount = m_entityManager.countEntitiesInHierarchy(rootEntity);
        SPDLOG_DEBUG("Entity {} hierarchy contains {} entities", rootEntity.id, hierarchyCount);

        entitiesJson.push_back(m_entityManager.serializeEntityHierarchy(rootEntity));
    }

    // Additional debug: Check if there are any orphaned entities
    const auto& allEntities = m_entityManager.getAllEntities();
    size_t totalEntities = allEntities.size();
    size_t rootEntitiesCount = rootEntities.size();

    // Count all entities that should be in hierarchies
    size_t entitiesInHierarchies = 0;
    for (Entity rootEntity : rootEntities) {
        entitiesInHierarchies += m_entityManager.countEntitiesInHierarchy(rootEntity);
    }

    SPDLOG_INFO("Scene stats: {} total entities, {} root entities, {} entities in hierarchies",
        totalEntities, rootEntitiesCount, entitiesInHierarchies);

    if (totalEntities != entitiesInHierarchies) {
        SPDLOG_WARN("Potential issue: {} entities may not be properly organized in hierarchies",
            totalEntities - entitiesInHierarchies);

        // Log details of entities that might be missing
        for (Entity entity : allEntities) {
            bool isRoot = std::find(rootEntities.begin(), rootEntities.end(), entity) != rootEntities.end();
            bool hasParent = m_entityManager.hasParent(entity);

            if (!isRoot && !hasParent) {
                SPDLOG_WARN("Entity {} ({}) is neither root nor has parent - this might indicate a problem",
                    entity.id, m_entityManager.getEntityName(entity));
            }
        }
    }

    return sceneData;
}

bool SceneRegistry::deserializeScene(const nlohmann::json& sceneData) {
    try {
        if (!sceneData.contains("entities")) {
            SPDLOG_WARN("Scene data contains no entities");
            return true; // Empty scene is valid
        }

        const auto& entitiesJson = sceneData["entities"];
        SPDLOG_INFO("Deserializing scene with {} entity hierarchies", entitiesJson.size());

        // Deserialize each entity hierarchy using the correct method
        for (size_t i = 0; i < entitiesJson.size(); ++i) {
            const auto& hierarchyJson = entitiesJson[i];
            SPDLOG_DEBUG("Deserializing hierarchy {}", i);

            Entity rootEntity = m_entityManager.deserializeEntityHierarchy(hierarchyJson);
            SPDLOG_DEBUG("Created root entity {} ({})", rootEntity.id, m_entityManager.getEntityName(rootEntity));
        }

        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to deserialize scene: {}", e.what());
        return false;
    }
}