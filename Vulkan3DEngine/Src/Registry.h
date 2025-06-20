#pragma once
#include <vector>
#include <memory>
#include <unordered_set>
#include <string>
#include "Entity.h"
#include "Component.h"

#include "SystemManager.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include "Serializer.h"
#include "PrefabInstanceManager.h"
#include "SceneRegistry.h"
#include "AssetSystem.h"

// Forward declarations
class SystemManager;
class EntityManager;
class ComponentManager;
class Serializer;
class PrefabInstanceManager;
class SceneRegistry;
class AssetSystem;

class Registry {
public:
    Registry(AssetSystem& assetSystem);

    // === PUBLIC API - Complex operations coordinated by Registry ===

    // Entity operations
    Entity create(const std::string& name = "");
    void destroy(Entity entity);
    bool valid(Entity entity) const;
    Entity cloneEntityHierarchy(Entity sourceEntity, Entity newParent = Entity(0), const std::string& name = "");
    void destroyAllEntities();

    // Prefab opeations (facade)
    void destroyAllPrefabInstances();

    // Scene operations (facade)
    bool loadScene(const std::string& sceneName);
    bool saveScene(const std::string& sceneName);
    void clearScene();
    std::string getCurrentSceneName() const;
    bool hasCurrentScene() const;

    // Scene validation
    bool validateScene() const;

    template<typename... Components>
    std::vector<Entity> createView();

    // Entity naming facade
    std::string getEntityName(Entity entity) const;
    void setEntityName(Entity entity, const std::string& name);
    Entity findEntityByName(const std::string& name) const;

    // Hierarchy facade
    void setParent(Entity child, Entity parent);
    Entity getParent(Entity entity) const;
    bool hasParent(Entity entity) const;
    const std::unordered_set<Entity>& getChildren(Entity entity) const;
    std::vector<Entity> getAllChildren(Entity entity) const;

    // Component facade - simplified without setRegistry call
    template<typename T, typename... Args>
    T& addComponent(Entity entity, Args&&... args);

    template<typename T>
    void removeComponent(Entity entity);

    template<typename T>
    T& getComponent(Entity entity);

    template<typename T>
    bool hasComponent(Entity entity) const;

    // Serialization facade
    nlohmann::json serializeEntity(Entity entity) const;
    Entity deserializeEntity(const nlohmann::json& entityData, Entity parent = Entity(0));

    // === DIRECT MODULE ACCESS ===

    SystemManager& systems() { return *m_systemManager; }
    EntityManager& entities() { return *m_entityManager; }
    ComponentManager& components() { return *m_componentManager; }
    Serializer& serialization() { return *m_serializer; }
    PrefabInstanceManager& prefabs() { return *m_prefabInstanceManager; }
    SceneRegistry& scenes() { return *m_sceneRegistry; }

private:
    // Modules - Registry coordinates between them
    std::unique_ptr<SystemManager> m_systemManager;
    std::unique_ptr<EntityManager> m_entityManager;
    std::unique_ptr<ComponentManager> m_componentManager;
    std::unique_ptr<Serializer> m_serializer;
    std::unique_ptr<PrefabInstanceManager> m_prefabInstanceManager;
    std::unique_ptr<SceneRegistry> m_sceneRegistry;

    // Helper methods
    bool canDestroyEntity(Entity entity) const;
    void validateEntityDestruction(const std::vector<Entity>& entities) const;
};

template<typename... Components>
std::vector<Entity> Registry::createView() {
    // Get all entities from EntityManager
    const auto& allEntities = m_entityManager->getAllEntities();

    // Convert set to vector and delegate to ComponentManager
    std::vector<Entity> entityVector(allEntities.begin(), allEntities.end());
    return m_componentManager->createView<Components...>(entityVector);
}

// Simplified addComponent - ComponentManager handles registry assignment
template<typename T, typename... Args>
T& Registry::addComponent(Entity entity, Args&&... args) {
    return m_componentManager->addComponent<T>(entity, std::forward<Args>(args)...);
}

template<typename T>
void Registry::removeComponent(Entity entity) {
    m_componentManager->removeComponent<T>(entity);
}

template<typename T>
T& Registry::getComponent(Entity entity) {
    return m_componentManager->getComponent<T>(entity);
}

template<typename T>
bool Registry::hasComponent(Entity entity) const {
    return m_componentManager->hasComponent<T>(entity);
}