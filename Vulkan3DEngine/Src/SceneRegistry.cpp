#include "SceneRegistry.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include "SceneManager.h"
#include "PrefabInstanceManager.h"
#include <spdlog/spdlog.h>

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
        SPDLOG_WARN("Scene name cannot be empty");
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

void SceneRegistry::clearScene() {
    // Najpierw zniszcz wszystkie instancje prefabów
    m_prefabInstanceManager.destroyAllPrefabInstances();

    // Następnie zniszcz pozostałe entity
    m_entityManager.destroyAllEntities();
}

nlohmann::json SceneRegistry::serializeCurrentScene() const {
    nlohmann::json sceneData;
    nlohmann::json& prefabInstancesJson = sceneData["prefabInstances"];
    nlohmann::json& entitiesJson = sceneData["entities"];

    // Pobierz wszystkie root entities
    std::vector<Entity> rootEntities = m_entityManager.getRootEntities();
    std::unordered_set<Entity> processedEntities;

    SPDLOG_INFO("Serializing scene with {} root entities", rootEntities.size());

    // Sortuj root entities dla przewidywalnej kolejności
    std::sort(rootEntities.begin(), rootEntities.end(),
        [](const Entity& a, const Entity& b) { return a.id < b.id; });

    // NAJPIERW serializuj instancje prefabów
    std::vector<PrefabInstanceHandle> allInstances = m_prefabInstanceManager.getAllInstances();
    SPDLOG_INFO("Found {} prefab instances to serialize", allInstances.size());

    for (PrefabInstanceHandle instanceHandle : allInstances) {
        const PrefabInstance* instance = m_prefabInstanceManager.getInstance(instanceHandle);
        if (!instance) {
            continue;
        }

        // Sprawdź czy root entity instancji jest root entity w scenie
        if (std::find(rootEntities.begin(), rootEntities.end(), instance->rootEntity) != rootEntities.end()) {
            nlohmann::json instanceJson = serializePrefabInstance(*instance);
            prefabInstancesJson.push_back(instanceJson);

            // Zaznacz wszystkie entity z tej instancji jako przetworzone
            for (Entity entity : instance->entities) {
                processedEntities.insert(entity);
            }

            SPDLOG_DEBUG("Serialized prefab instance: {} (root entity: {})",
                m_prefabInstanceManager.getInstanceName(instanceHandle), instance->rootEntity.id);
        }
    }

    // POTEM serializuj pozostałe zwykłe entity (które nie są częścią instancji prefabów)
    for (Entity rootEntity : rootEntities) {
        // Pomiń entity które już zostały przetworzone jako część instancji prefabów
        if (processedEntities.find(rootEntity) != processedEntities.end()) {
            continue;
        }

        SPDLOG_DEBUG("Serializing regular root entity {} ({})", rootEntity.id,
            m_entityManager.getEntityName(rootEntity));

        // Sprawdź czy jakaś entity w hierarchii jest częścią instancji prefabu
        if (isEntityHierarchyPartOfPrefabInstance(rootEntity)) {
            SPDLOG_WARN("Entity hierarchy {} contains prefab instance entities - this may cause issues",
                rootEntity.id);
        }

        entitiesJson.push_back(m_entityManager.serializeEntityHierarchy(rootEntity));
    }

    // Debug statistics
    SPDLOG_INFO("Scene serialization complete: {} prefab instances, {} regular entity hierarchies",
        prefabInstancesJson.size(), entitiesJson.size());

    return sceneData;
}

bool SceneRegistry::deserializeScene(const nlohmann::json& sceneData) {
    try {
        // NAJPIERW deserializuj instancje prefabów
        if (sceneData.contains("prefabInstances")) {
            const auto& prefabInstancesJson = sceneData["prefabInstances"];
            SPDLOG_INFO("Deserializing {} prefab instances", prefabInstancesJson.size());

            for (size_t i = 0; i < prefabInstancesJson.size(); ++i) {
                const auto& instanceJson = prefabInstancesJson[i];
                if (!deserializePrefabInstance(instanceJson)) {
                    SPDLOG_ERROR("Failed to deserialize prefab instance {}", i);
                    return false;
                }
            }
        }

        // POTEM deserializuj zwykłe entity
        if (sceneData.contains("entities")) {
            const auto& entitiesJson = sceneData["entities"];
            SPDLOG_INFO("Deserializing {} regular entity hierarchies", entitiesJson.size());

            for (size_t i = 0; i < entitiesJson.size(); ++i) {
                const auto& entityHierarchyJson = entitiesJson[i];
                SPDLOG_DEBUG("Deserializing regular entity hierarchy {}", i);

                Entity rootEntity = m_entityManager.deserializeEntityHierarchy(entityHierarchyJson);
                if (rootEntity.id == 0) {
                    SPDLOG_ERROR("Failed to deserialize entity hierarchy {}", i);
                    return false;
                }

                SPDLOG_DEBUG("Created regular root entity {} ({})", rootEntity.id,
                    m_entityManager.getEntityName(rootEntity));
            }
        }

        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to deserialize scene: {}", e.what());
        return false;
    }
}

nlohmann::json SceneRegistry::serializePrefabInstance(const PrefabInstance& instance) const {
    nlohmann::json instanceJson;

    // Podstawowe informacje o instancji
    instanceJson["instanceId"] = instance.handle.id;
    instanceJson["prefabId"] = instance.prefabHandle.id;
    instanceJson["instanceName"] = m_prefabInstanceManager.getInstanceName(instance.handle);
    instanceJson["rootEntityId"] = instance.rootEntity.id;

    // Pozycja i transformacja root entity (jeśli ma parent)
    if (m_entityManager.hasParent(instance.rootEntity)) {
        Entity parent = m_entityManager.getParent(instance.rootEntity);
        instanceJson["parentEntityId"] = parent.id;
        // Uwaga: parentEntityId odnosi się do entity które może być częścią innej instancji
        // lub zwykłym entity - system musi to obsłużyć podczas deserializacji
    }

    // Serialize overridden components
    if (!instance.overriddenComponents.empty()) {
        nlohmann::json& overridesJson = instanceJson["componentOverrides"];

        for (const auto& [entity, overriddenComponentTypes] : instance.overriddenComponents) {
            if (overriddenComponentTypes.empty()) {
                continue;
            }

            nlohmann::json entityOverrideJson;
            entityOverrideJson["entityId"] = entity.id;

            // Serialize each overridden component with its current data
            nlohmann::json& componentsJson = entityOverrideJson["components"];
            for (const std::string& componentType : overriddenComponentTypes) {
                try {
                    nlohmann::json componentData = m_componentManager.serializeComponent(entity, componentType);
                    componentsJson[componentType] = componentData;
                }
                catch (const std::exception& e) {
                    SPDLOG_ERROR("Failed to serialize overridden component {} for entity {}: {}",
                        componentType, entity.id, e.what());
                }
            }

            overridesJson.push_back(entityOverrideJson);
        }
    }

    return instanceJson;
}

bool SceneRegistry::deserializePrefabInstance(const nlohmann::json& instanceJson) {
    try {
        // Sprawdź wymagane pola
        if (!instanceJson.contains("prefabId") || !instanceJson.contains("instanceName")) {
            SPDLOG_ERROR("Invalid prefab instance data - missing required fields");
            return false;
        }

        PrefabHandle prefabHandle(instanceJson["prefabId"].get<uint32_t>());
        std::string instanceName = instanceJson["instanceName"].get<std::string>();

        // Określ parent entity (jeśli istnieje)
        Entity parentEntity(0);
        if (instanceJson.contains("parentEntityId")) {
            uint32_t parentId = instanceJson["parentEntityId"].get<uint32_t>();
            parentEntity = Entity(parentId);

            // Sprawdź czy parent entity istnieje
            if (!m_entityManager.valid(parentEntity)) {
                SPDLOG_ERROR("Parent entity {} for prefab instance {} does not exist",
                    parentId, instanceName);
                return false;
            }
        }

        // Utwórz instancję prefabu
        PrefabInstanceHandle instanceHandle = m_prefabInstanceManager.createInstance(
            prefabHandle, parentEntity);

        if (instanceHandle.id == 0) {
            SPDLOG_ERROR("Failed to create prefab instance {}", instanceName);
            return false;
        }

        // Deserializuj overridden components
        if (instanceJson.contains("componentOverrides")) {
            const auto& overridesJson = instanceJson["componentOverrides"];

            for (const auto& entityOverrideJson : overridesJson) {
                if (!entityOverrideJson.contains("entityId") || !entityOverrideJson.contains("components")) {
                    continue;
                }

                uint32_t originalEntityId = entityOverrideJson["entityId"].get<uint32_t>();

                // Znajdź odpowiadającą entity w nowej instancji
                // To jest skomplikowane - potrzebujemy mapowania starych ID na nowe
                Entity newEntity = findCorrespondingEntityInInstance(instanceHandle, originalEntityId);
                if (newEntity.id == 0) {
                    SPDLOG_WARN("Could not find corresponding entity for original ID {} in instance {}",
                        originalEntityId, instanceName);
                    continue;
                }

                const auto& componentsJson = entityOverrideJson["components"];
                for (const auto& [componentType, componentData] : componentsJson.items()) {
                    // Override component w instancji
                    if (m_prefabInstanceManager.overrideComponent(instanceHandle, newEntity, componentType)) {
                        // Deserializuj override data
                        try {
                            m_componentManager.deserializeComponent(newEntity, componentType, componentData);
                            SPDLOG_DEBUG("Restored component override {} for entity {} in instance {}",
                                componentType, newEntity.id, instanceName);
                        }
                        catch (const std::exception& e) {
                            SPDLOG_ERROR("Failed to deserialize overridden component {} for entity {}: {}",
                                componentType, newEntity.id, e.what());
                        }
                    }
                }
            }
        }

        SPDLOG_DEBUG("Successfully deserialized prefab instance: {}", instanceName);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to deserialize prefab instance: {}", e.what());
        return false;
    }
}

Entity SceneRegistry::findCorrespondingEntityInInstance(PrefabInstanceHandle instanceHandle, uint32_t originalEntityId) const {
    // To jest uproszczona implementacja - w rzeczywistości potrzebujesz bardziej sofistykowanego
    // mapowania, np. na podstawie nazw entity lub pozycji w hierarchii

    const PrefabInstance* instance = m_prefabInstanceManager.getInstance(instanceHandle);
    if (!instance) {
        return Entity(0);
    }

    // Jeśli to root entity, zwróć root entity instancji
    if (originalEntityId == instance->rootEntity.id) {
        return instance->rootEntity;
    }

    // Dla innych entities - możesz użyć nazw lub innych identyfikatorów
    // Na razie zwracamy pierwszą pasującą entity (to nie jest idealne rozwiązanie)
    for (Entity entity : instance->entities) {
        // Tu powinieneś mieć lepszą logikę mapowania
        // Na przykład porównanie nazw entity lub pozycji w hierarchii
        if (entity.id == originalEntityId) {
            return entity; // To prawdopodobnie się nie zdarzy, ale...
        }
    }

    return Entity(0);
}

bool SceneRegistry::isEntityHierarchyPartOfPrefabInstance(Entity rootEntity) const {
    // Sprawdź czy rootEntity lub którekolwiek z jego dzieci jest częścią instancji prefabu
    if (m_prefabInstanceManager.isEntityPartOfInstance(rootEntity)) {
        return true;
    }

    // Sprawdź rekurencyjnie wszystkie dzieci
    const auto& children = m_entityManager.getChildren(rootEntity);
    for (Entity child : children) {
        if (isEntityHierarchyPartOfPrefabInstance(child)) {
            return true;
        }
    }

    return false;
}