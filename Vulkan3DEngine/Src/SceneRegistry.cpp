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
	clearScene();
	if (sceneName.empty()) {
		SPDLOG_WARN("Scene name cannot empty");
		return false;
	}
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

    // Pobierz wszystkie root entities
    std::vector<Entity> rootEntities = m_entityManager.getRootEntities();

    SPDLOG_INFO("Serializing scene with {} root entities", rootEntities.size());

    // Sortuj root entities dla przewidywalnej kolejności
    std::sort(rootEntities.begin(), rootEntities.end(),
        [](const Entity& a, const Entity& b) { return a.id < b.id; });

    // Serializuj każdą hierarchię root entity
    for (Entity rootEntity : rootEntities) {
        SPDLOG_DEBUG("Serializing root entity {} ({})", rootEntity.id,
            m_entityManager.getEntityName(rootEntity));

        uint32_t hierarchyCount = m_entityManager.countEntitiesInHierarchy(rootEntity);
        SPDLOG_DEBUG("Entity {} hierarchy contains {} entities", rootEntity.id, hierarchyCount);

        // UŻYWAJ serializeEntityHierarchy dla kompletnej hierarchii
        entitiesJson.push_back(m_entityManager.serializeEntityHierarchy(rootEntity));
    }

    // Debug statistics
    const auto& allEntities = m_entityManager.getAllEntities();
    size_t totalEntities = allEntities.size();
    size_t rootEntitiesCount = rootEntities.size();

    size_t entitiesInHierarchies = 0;
    for (Entity rootEntity : rootEntities) {
        entitiesInHierarchies += m_entityManager.countEntitiesInHierarchy(rootEntity);
    }

    SPDLOG_INFO("Scene stats: {} total entities, {} root entities, {} entities in hierarchies",
        totalEntities, rootEntitiesCount, entitiesInHierarchies);

    if (totalEntities != entitiesInHierarchies) {
        SPDLOG_WARN("Warning: {} entities may not be properly organized in hierarchies",
            totalEntities - entitiesInHierarchies);
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

        // Deserializuj każdą hierarchię root entity
        for (size_t i = 0; i < entitiesJson.size(); ++i) {
            const auto& entityHierarchyJson = entitiesJson[i];
            SPDLOG_DEBUG("Deserializing entity hierarchy {}", i);

            // UŻYWAJ deserializeEntityHierarchy dla kompletnej hierarchii
            Entity rootEntity = m_entityManager.deserializeEntityHierarchy(entityHierarchyJson);
            SPDLOG_DEBUG("Created root entity {} ({})", rootEntity.id,
                m_entityManager.getEntityName(rootEntity));
        }

        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to deserialize scene: {}", e.what());
        return false;
    }
}