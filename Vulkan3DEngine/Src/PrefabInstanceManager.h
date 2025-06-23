#pragma once
#include "Entity.h"
#include "Handle.h"
#include "AssetLib.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include "Event.h"

// Forward declarations
class Registry;
struct Prefab;
class PrefabManager;
class EntityManager;
class ComponentManager;

// Simple component override tracking
struct ComponentOverride {
    std::string componentType;
    bool isOverridden = false;

    ComponentOverride(const std::string& type) : componentType(type) {}
};

// Prefab instance data
struct PrefabInstance {
    PrefabInstanceHandle handle;
    PrefabHandle prefabHandle;
    Entity rootEntity;
    std::unordered_set<Entity> entities; // All entities in this instance
    std::unordered_map<Entity, std::unordered_set<std::string>> overriddenComponents; // Track which components are overridden per entity
};

class PrefabInstanceManager {
public:
    explicit PrefabInstanceManager(EntityManager& entityManager, ComponentManager& componentManager,
        PrefabManager& prefabManager);
    ~PrefabInstanceManager() = default;

    // Instance creation
    PrefabInstanceHandle createInstance(PrefabHandle prefabHandle, Entity parent = Entity(0));
    PrefabInstanceHandle createInstance(const std::string& prefabName, Entity parent = Entity(0));

    // Instance management
    bool destroyInstance(PrefabInstanceHandle instanceHandle);
    bool unpackInstance(PrefabInstanceHandle instanceHandle); // Makes entities "free"

    // Instance queries
    bool isValidInstance(PrefabInstanceHandle instanceHandle) const;
    const PrefabInstance* getInstance(PrefabInstanceHandle instanceHandle) const;
    PrefabInstanceHandle findInstanceByEntity(Entity entity) const;
    std::vector<PrefabInstanceHandle> getAllInstances() const;

    // Entity management
    bool isEntityPartOfInstance(Entity entity) const;
    bool isInstanceRoot(Entity entity) const;
    PrefabInstanceHandle getInstanceForEntity(Entity entity) const;

    // Component overrides - Copy-on-Write system
    bool overrideComponent(PrefabInstanceHandle instanceHandle, Entity entity, const std::string& componentType);
    bool restoreComponent(PrefabInstanceHandle instanceHandle, Entity entity, const std::string& componentType);
    bool isComponentOverridden(PrefabInstanceHandle instanceHandle, Entity entity, const std::string& componentType) const;

    // Bulk operations
    bool overrideAllComponents(PrefabInstanceHandle instanceHandle, Entity entity);
    bool restoreAllComponents(PrefabInstanceHandle instanceHandle, Entity entity);
    std::unordered_set<std::string> getOverriddenComponents(PrefabInstanceHandle instanceHandle, Entity entity) const;
    void destroyAllPrefabInstances();

    // Prefab synchronization
    bool syncWithPrefab(PrefabInstanceHandle instanceHandle); // Updates non-overridden components from prefab

    // Instance properties
    std::string getInstanceName(PrefabInstanceHandle instanceHandle) const;

    // Cleanup - called by Registry when entity is being destroyed
    void onEntityDestroyed(Entity entity);

    // Prefab creation and saving
    PrefabHandle createPrefabFromEntity(Entity rootEntity);
    bool saveInstanceAsPrefab(PrefabInstanceHandle instanceHandle);

private:
    // Module references
    EntityManager& m_entityManager;
    ComponentManager& m_componentManager;
    PrefabManager& m_prefabManager;

    // Instance storage
    std::unordered_map<uint32_t, PrefabInstance> m_instances;
    std::unordered_map<Entity, PrefabInstanceHandle> m_entityToInstance;

    // Handle generation
    uint32_t m_nextInstanceId = 1;

    // Helper methods
    PrefabInstanceHandle generateHandle();
    void collectEntitiesInHierarchy(Entity entity, std::unordered_set<Entity>& entities);
    Entity instantiatePrefabInternal(PrefabHandle prefabHandle, Entity parent);
    void registerInstanceEntities(PrefabInstanceHandle instanceHandle, Entity rootEntity);
    void unregisterInstanceEntities(PrefabInstanceHandle instanceHandle);

    // Helper method for converting existing entities to prefab instance
    PrefabInstanceHandle convertEntitiesToInstance(PrefabHandle prefabHandle, Entity rootEntity,
        const std::unordered_set<Entity>& entities);

    // Copy-on-write helpers
    bool restoreComponentFromPrefab(Entity entity, const std::string& componentType, PrefabHandle prefabHandle);
    void buildEntityMapping(Entity instanceRoot, Entity prefabRoot,
        std::unordered_map<Entity, Entity>& mapping, const Prefab& prefab);

    void syncEntityWithPrefab(Entity instanceEntity, Entity prefabEntity, const PrefabInstance& instance);

    Event<Entity>::Subscription m_entityDestroyedSubscription;
};