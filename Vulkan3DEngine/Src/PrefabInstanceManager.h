#pragma once
#include "Entity.h"
#include "Handle.h"
#include "AssetLib.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

// Forward declarations
class Registry;
struct Prefab;

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
    std::string instanceName;
};

class PrefabInstanceManager {
public:
    explicit PrefabInstanceManager(Registry& registry);
    ~PrefabInstanceManager() = default;

    // Instance creation
    PrefabInstanceHandle createInstance(PrefabHandle prefabHandle, Entity parent = Entity(0),
        const std::string& instanceName = "");

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
    bool canDestroyEntity(Entity entity) const; // Returns false if entity is part of locked instance
    PrefabInstanceHandle getInstanceForEntity(Entity entity) const;

    // Component overrides - Copy-on-Write system
    bool overrideComponent(PrefabInstanceHandle instanceHandle, Entity entity, const std::string& componentType);
    bool restoreComponent(PrefabInstanceHandle instanceHandle, Entity entity, const std::string& componentType);
    bool isComponentOverridden(PrefabInstanceHandle instanceHandle, Entity entity, const std::string& componentType) const;

    // Bulk operations
    bool overrideAllComponents(PrefabInstanceHandle instanceHandle, Entity entity);
    bool restoreAllComponents(PrefabInstanceHandle instanceHandle, Entity entity);
    std::unordered_set<std::string> getOverriddenComponents(PrefabInstanceHandle instanceHandle, Entity entity) const;

    // Prefab synchronization
    bool syncWithPrefab(PrefabInstanceHandle instanceHandle); // Updates non-overridden components from prefab

    // Instance properties
    std::string getInstanceName(PrefabInstanceHandle instanceHandle) const;
    void setInstanceName(PrefabInstanceHandle instanceHandle, const std::string& name);

    // Cleanup - called by Registry when entity is being destroyed
    void onEntityDestroyed(Entity entity);

    // Prefab creation from entity
    PrefabHandle createPrefabFromEntity(Entity rootEntity);

private:
    Registry& m_registry;

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

    // Copy-on-write helpers
    bool restoreComponentFromPrefab(Entity entity, const std::string& componentType, PrefabHandle prefabHandle);
    void syncEntityWithPrefab(Entity instanceEntity, Entity prefabEntity, const PrefabInstance& instance);
};