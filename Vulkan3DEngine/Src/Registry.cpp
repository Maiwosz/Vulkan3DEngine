#include "Registry.h"
#include "SystemManager.h"
#include "Engine.h"
#include <stdexcept>
#include <algorithm>
#include <spdlog/spdlog.h>

Registry::Registry(Engine& engine) {
    // Create modules in dependency order
    m_systemManager = std::make_unique<SystemManager>(*this, engine);
    m_componentManager = std::make_unique<ComponentManager>(*this);
    m_entityManager = std::make_unique<EntityManager>(*m_componentManager);
    m_serializer = std::make_unique<Serializer>(*m_entityManager, *m_componentManager);
    m_prefabInstanceManager = std::make_unique<PrefabInstanceManager>(
        *m_entityManager,
        *m_componentManager,
        *m_serializer,
        engine.assetSystem().prefabManager()
    );
    m_sceneRegistry = std::make_unique<SceneRegistry>(
        *m_entityManager,
        *m_componentManager,
        *m_serializer,
        engine.assetSystem().sceneManager()
    );
}

// === PUBLIC API - Complex operations coordinated by Registry ===

Entity Registry::create(const std::string& name) {
    return m_entityManager->create(name);
}

void Registry::destroy(Entity entity) {
    if (!canDestroyEntity(entity)) {
        SPDLOG_WARN("Cannot destroy entity {} - validation failed", entity.id);
        return;
    }

    // Sprawdź czy entity jest korzeniem instancji prefaba
    if (m_prefabInstanceManager->isEntityPartOfInstance(entity)) {
        PrefabInstanceHandle instanceHandle = m_prefabInstanceManager->getInstanceForEntity(entity);
        const PrefabInstance* instance = m_prefabInstanceManager->getInstance(instanceHandle);

        if (instance && instance->rootEntity == entity) {
            // Jeśli to korzeń instancji - zniszcz całą instancję
            SPDLOG_INFO("Destroying prefab instance {} via root entity {}", instanceHandle.id, entity.id);
            m_prefabInstanceManager->destroyInstance(instanceHandle);
            return;
        }
        else {
            // Jeśli to nie korzeń - nie pozwalaj na usunięcie
            SPDLOG_WARN("Cannot destroy entity {} - it's part of prefab instance but not root", entity.id);
            return;
        }
    }

    // Standardowe usuwanie entity
    m_entityManager->destroy(entity);
}

bool Registry::valid(Entity entity) const {
    return m_entityManager->valid(entity);
}

Entity Registry::cloneEntityHierarchy(Entity sourceEntity, Entity newParent, const std::string& name) {
    if (!m_entityManager->valid(sourceEntity)) {
        return Entity(0);
    }

    // Determine final name
    std::string finalName = name;
    if (finalName.empty()) {
        std::string sourceName = m_entityManager->getEntityName(sourceEntity);
        finalName = sourceName.empty() ? "Entity_Copy" : sourceName + "_Copy";
    }

    // Create new entity
    Entity newEntity = m_entityManager->create(finalName);

    // Set parent if provided
    if (newParent.id != 0 && m_entityManager->valid(newParent)) {
        m_entityManager->setParent(newEntity, newParent);
    }

    // Copy all components using serialization
    auto componentTypes = m_componentManager->getEntityComponentTypes(sourceEntity);
    for (const std::string& typeName : componentTypes) {
        auto componentData = m_serializer->serializeComponent(sourceEntity, typeName);
        m_serializer->deserializeComponent(newEntity, typeName, componentData);

        // Set registry reference for the component
        // This is handled automatically by addComponent, but for deserialized components we need to set it manually
        // We'll need a way to get the component and set its registry - this could be added to ComponentManager
    }

    // Recursively clone children
    const auto& children = m_entityManager->getChildren(sourceEntity);
    for (Entity child : children) {
        cloneEntityHierarchy(child, newEntity); // Recursive clone
    }

    return newEntity;
}

void Registry::destroyAllEntities() {
    // Najpierw zniszcz wszystkie instancje prefabów (dla pewności)
    auto allInstances = m_prefabInstanceManager->getAllInstances();
    for (auto instanceHandle : allInstances) {
        m_prefabInstanceManager->destroyInstance(instanceHandle);
    }

    // Następnie zniszcz wszystkie pozostałe entity
    auto allEntities = m_entityManager->getAllEntities();
    std::vector<Entity> entitiesToDestroy(allEntities.begin(), allEntities.end());

    for (Entity entity : entitiesToDestroy) {
        if (m_entityManager->valid(entity)) {
            m_entityManager->destroy(entity);
        }
    }
}

void Registry::destroyAllPrefabInstances() {
    auto allInstances = m_prefabInstanceManager->getAllInstances();
    for (auto instanceHandle : allInstances) {
        m_prefabInstanceManager->destroyInstance(instanceHandle);
    }
}

// Scene operations (facade)
bool Registry::loadScene(const std::string& sceneName) {
    clearScene();
    return m_sceneRegistry->loadScene(sceneName);
}

bool Registry::saveScene(const std::string& sceneName) {
    return m_sceneRegistry->saveScene(sceneName);
}

void Registry::clearScene() {
    // Najpierw zniszcz wszystkie instancje prefabów
    destroyAllPrefabInstances();

    // Następnie zniszcz pozostałe entity
    destroyAllEntities();
}

std::string Registry::getCurrentSceneName() const {
    return m_sceneRegistry->getCurrentSceneName();
}

bool Registry::hasCurrentScene() const {
    return m_sceneRegistry->hasCurrentScene();
}

bool Registry::validateScene() const {
    // Validate entity-component consistency
    for (Entity entity : m_entityManager->getAllEntities()) {
        if (!m_entityManager->valid(entity)) {
            return false;
        }

        // Validate hierarchy consistency
        if (m_entityManager->hasParent(entity)) {
            Entity parent = m_entityManager->getParent(entity);
            if (!m_entityManager->valid(parent)) {
                return false;
            }

            const auto& parentChildren = m_entityManager->getChildren(parent);
            if (parentChildren.find(entity) == parentChildren.end()) {
                return false;
            }
        }
    }

    return true;
}

// Entity naming facade
std::string Registry::getEntityName(Entity entity) const {
    return m_entityManager->getEntityName(entity);
}

void Registry::setEntityName(Entity entity, const std::string& name) {
    m_entityManager->setEntityName(entity, name);
}

Entity Registry::findEntityByName(const std::string& name) const {
    return m_entityManager->findEntityByName(name);
}

// Hierarchy facade
void Registry::setParent(Entity child, Entity parent) {
    m_entityManager->setParent(child, parent);
}

Entity Registry::getParent(Entity entity) const {
    return m_entityManager->getParent(entity);
}

bool Registry::hasParent(Entity entity) const {
    return m_entityManager->hasParent(entity);
}

const std::unordered_set<Entity>& Registry::getChildren(Entity entity) const {
    return m_entityManager->getChildren(entity);
}

std::vector<Entity> Registry::getAllChildren(Entity entity) const {
    return m_entityManager->getAllChildren(entity);
}

// Serialization facade
nlohmann::json Registry::serializeEntity(Entity entity) const {
    return m_serializer->serializeEntity(entity);
}

Entity Registry::deserializeEntity(const nlohmann::json& entityData, Entity parent) {
    Entity entity = m_serializer->deserializeEntity(entityData, parent);

    // Set registry reference for all components of the deserialized entity
    // This would need to be handled by the serializer or component manager

    return entity;
}

// === HELPER METHODS ===

bool Registry::canDestroyEntity(Entity entity) const {
    if (!m_entityManager->valid(entity)) {
        return false;
    }

    // Sprawdź czy entity jest częścią instancji prefaba
    if (m_prefabInstanceManager->isEntityPartOfInstance(entity)) {
        PrefabInstanceHandle instanceHandle = m_prefabInstanceManager->getInstanceForEntity(entity);
        const PrefabInstance* instance = m_prefabInstanceManager->getInstance(instanceHandle);

        if (instance) {
            // Można usunąć tylko korzeń instancji (co usuwa całą instancję)
            return instance->rootEntity == entity;
        }
    }

    return true;
}

void Registry::validateEntityDestruction(const std::vector<Entity>& entities) const {
    for (Entity entity : entities) {
        if (!canDestroyEntity(entity)) {
            throw std::runtime_error("Cannot destroy entity " + std::to_string(entity.id));
        }
    }
}