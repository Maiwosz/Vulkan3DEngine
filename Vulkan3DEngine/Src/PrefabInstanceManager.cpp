#include "PrefabInstanceManager.h"
#include "Registry.h"
#include "PrefabManager.h"
#include <spdlog/spdlog.h>
#include "Engine.h"

PrefabInstanceManager::PrefabInstanceManager(Registry& registry)
    : m_registry(registry) {
}

PrefabInstanceHandle PrefabInstanceManager::createInstance(PrefabHandle prefabHandle, Entity parent,
    const std::string& instanceName) {
    try {
        // Create instance handle
        PrefabInstanceHandle instanceHandle = generateHandle();

        // Instantiate the prefab
        Entity rootEntity = instantiatePrefabInternal(prefabHandle, parent);
        if (rootEntity.id == 0) {
            SPDLOG_ERROR("Failed to instantiate prefab");
            return PrefabInstanceHandle{};
        }

        // Create instance data
        PrefabInstance instance;
        instance.handle = instanceHandle;
        instance.prefabHandle = prefabHandle;
        instance.rootEntity = rootEntity;
        instance.instanceName = instanceName.empty() ?
            ("Instance_" + std::to_string(instanceHandle.id)) : instanceName;

        // Register all entities in the instance
        registerInstanceEntities(instanceHandle, rootEntity);

        // Store instance
        m_instances[instanceHandle.id] = std::move(instance);

        SPDLOG_DEBUG("Created prefab instance: {} with root entity: {}",
            instance.instanceName, rootEntity.id);

        return instanceHandle;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to create prefab instance: {}", e.what());
        return PrefabInstanceHandle{};
    }
}

bool PrefabInstanceManager::destroyInstance(PrefabInstanceHandle instanceHandle) {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return false;
    }

    PrefabInstance& instance = it->second;

    // Unregister entities
    unregisterInstanceEntities(instanceHandle);

    // Destroy all entities in the instance (starting from leaves)
    std::vector<Entity> entitiesToDestroy(instance.entities.begin(), instance.entities.end());

    // Sort by hierarchy depth (children first)
    std::sort(entitiesToDestroy.begin(), entitiesToDestroy.end(),
        [this](Entity a, Entity b) {
            return m_registry.getChildren(a).empty() && !m_registry.getChildren(b).empty();
        });

    for (Entity entity : entitiesToDestroy) {
        if (m_registry.valid(entity)) {
            m_registry.destroy(entity);
        }
    }

    // Remove instance
    m_instances.erase(it);

    SPDLOG_DEBUG("Destroyed prefab instance: {}", instanceHandle.id);
    return true;
}

bool PrefabInstanceManager::unpackInstance(PrefabInstanceHandle instanceHandle) {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return false;
    }

    // Unregister entities (they become "free")
    unregisterInstanceEntities(instanceHandle);

    // Remove instance data but keep entities
    m_instances.erase(it);

    SPDLOG_DEBUG("Unpacked prefab instance: {}", instanceHandle.id);
    return true;
}

// Copy-on-Write Component Override System

bool PrefabInstanceManager::overrideComponent(PrefabInstanceHandle instanceHandle, Entity entity,
    const std::string& componentType) {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return false;
    }

    PrefabInstance& instance = it->second;

    // Check if entity belongs to this instance
    if (instance.entities.find(entity) == instance.entities.end()) {
        return false;
    }

    // Check if entity has this component
    if (!m_registry.getComponentPool(componentType) ||
        !m_registry.getComponentPool(componentType)->hasEntity(entity)) {
        SPDLOG_WARN("Entity {} doesn't have component {}", entity.id, componentType);
        return false;
    }

    // Mark component as overridden
    instance.overriddenComponents[entity].insert(componentType);

    SPDLOG_DEBUG("Overrode component {} for entity {} in instance {}",
        componentType, entity.id, instanceHandle.id);
    return true;
}

bool PrefabInstanceManager::restoreComponent(PrefabInstanceHandle instanceHandle, Entity entity,
    const std::string& componentType) {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return false;
    }

    PrefabInstance& instance = it->second;

    // Check if entity belongs to this instance
    if (instance.entities.find(entity) == instance.entities.end()) {
        return false;
    }

    // Check if component is actually overridden
    auto entityOverrideIt = instance.overriddenComponents.find(entity);
    if (entityOverrideIt == instance.overriddenComponents.end() ||
        entityOverrideIt->second.find(componentType) == entityOverrideIt->second.end()) {
        return false; // Component wasn't overridden
    }

    // Restore component from prefab
    if (!restoreComponentFromPrefab(entity, componentType, instance.prefabHandle)) {
        SPDLOG_ERROR("Failed to restore component {} for entity {}", componentType, entity.id);
        return false;
    }

    // Remove override flag
    entityOverrideIt->second.erase(componentType);
    if (entityOverrideIt->second.empty()) {
        instance.overriddenComponents.erase(entityOverrideIt);
    }

    SPDLOG_DEBUG("Restored component {} for entity {} in instance {}",
        componentType, entity.id, instanceHandle.id);
    return true;
}

bool PrefabInstanceManager::isComponentOverridden(PrefabInstanceHandle instanceHandle, Entity entity,
    const std::string& componentType) const {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return false;
    }

    const PrefabInstance& instance = it->second;
    auto entityOverrideIt = instance.overriddenComponents.find(entity);
    if (entityOverrideIt == instance.overriddenComponents.end()) {
        return false;
    }

    return entityOverrideIt->second.find(componentType) != entityOverrideIt->second.end();
}

bool PrefabInstanceManager::overrideAllComponents(PrefabInstanceHandle instanceHandle, Entity entity) {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return false;
    }

    PrefabInstance& instance = it->second;

    // Check if entity belongs to this instance
    if (instance.entities.find(entity) == instance.entities.end()) {
        return false;
    }

    // Get all component types for this entity
    std::vector<std::string> componentTypes = m_registry.getEntityComponentTypes(entity);

    // Mark all components as overridden
    auto& entityOverrides = instance.overriddenComponents[entity];
    for (const std::string& componentType : componentTypes) {
        entityOverrides.insert(componentType);
    }

    SPDLOG_DEBUG("Overrode all components for entity {} in instance {}",
        entity.id, instanceHandle.id);
    return true;
}

bool PrefabInstanceManager::restoreAllComponents(PrefabInstanceHandle instanceHandle, Entity entity) {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return false;
    }

    PrefabInstance& instance = it->second;

    // Check if entity belongs to this instance
    if (instance.entities.find(entity) == instance.entities.end()) {
        return false;
    }

    auto entityOverrideIt = instance.overriddenComponents.find(entity);
    if (entityOverrideIt == instance.overriddenComponents.end()) {
        return true; // Nothing to restore
    }

    // Restore all overridden components
    std::vector<std::string> componentsToRestore;
    componentsToRestore.reserve(entityOverrideIt->second.size());

    for (const std::string& componentType : entityOverrideIt->second) {
        componentsToRestore.push_back(componentType);
    }

    bool allRestored = true;
    for (const std::string& componentType : componentsToRestore) {
        if (!restoreComponentFromPrefab(entity, componentType, instance.prefabHandle)) {
            SPDLOG_ERROR("Failed to restore component {} for entity {}", componentType, entity.id);
            allRestored = false;
        }
    }

    // Clear all overrides for this entity
    instance.overriddenComponents.erase(entityOverrideIt);

    SPDLOG_DEBUG("Restored all components for entity {} in instance {}",
        entity.id, instanceHandle.id);
    return allRestored;
}

std::unordered_set<std::string> PrefabInstanceManager::getOverriddenComponents(
    PrefabInstanceHandle instanceHandle, Entity entity) const {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return {};
    }

    const PrefabInstance& instance = it->second;
    auto entityOverrideIt = instance.overriddenComponents.find(entity);
    if (entityOverrideIt == instance.overriddenComponents.end()) {
        return {};
    }

    return entityOverrideIt->second;
}

bool PrefabInstanceManager::syncWithPrefab(PrefabInstanceHandle instanceHandle) {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return false;
    }

    PrefabInstance& instance = it->second;

    // Get the prefab data
    const Prefab* prefab = Engine::get().assetSystem().prefabManager().getPrefab(instance.prefabHandle);
    if (!prefab) {
        SPDLOG_ERROR("Prefab not found for sync operation");
        return false;
    }

    // Create mapping from instance entities to prefab entities
    // This is a simplified approach - in practice you might need more sophisticated mapping
    std::unordered_map<Entity, Entity> instanceToPrefabMapping;

    // For now we'll just sync the root entity and use hierarchy traversal
    instanceToPrefabMapping[instance.rootEntity] = prefab->rootEntity;

    // Build complete mapping by traversing both hierarchies in parallel
    std::function<void(Entity, Entity)> buildMapping = [&](Entity instanceEnt, Entity prefabEnt) {
        instanceToPrefabMapping[instanceEnt] = prefabEnt;

        const auto& instanceChildren = m_registry.getChildren(instanceEnt);
        const auto& prefabChildren = m_registry.getChildren(prefabEnt);

        // Simple mapping - assumes children are in same order
        // More sophisticated mapping would use names or other identifiers
        auto instanceIt = instanceChildren.begin();
        auto prefabIt = prefabChildren.begin();

        while (instanceIt != instanceChildren.end() && prefabIt != prefabChildren.end()) {
            buildMapping(*instanceIt, *prefabIt);
            ++instanceIt;
            ++prefabIt;
        }
        };

    buildMapping(instance.rootEntity, prefab->rootEntity);

    // Sync each entity
    bool allSynced = true;
    for (Entity instanceEntity : instance.entities) {
        auto mappingIt = instanceToPrefabMapping.find(instanceEntity);
        if (mappingIt != instanceToPrefabMapping.end()) {
            syncEntityWithPrefab(instanceEntity, mappingIt->second, instance);
        }
        else {
            SPDLOG_WARN("Could not find prefab mapping for instance entity {}", instanceEntity.id);
            allSynced = false;
        }
    }

    SPDLOG_DEBUG("Synced instance {} with prefab", instanceHandle.id);
    return allSynced;
}

// Helper Methods Implementation

bool PrefabInstanceManager::restoreComponentFromPrefab(Entity entity, const std::string& componentType,
    PrefabHandle prefabHandle) {
    try {
        const Prefab* prefab = Engine::get().assetSystem().prefabManager().getPrefab(prefabHandle);
        if (!prefab) {
            return false;
        }

        // Find the corresponding prefab entity
        // This is simplified - in practice you'd need proper entity mapping
        Entity prefabEntity = prefab->rootEntity; // Simplified mapping

        // Find prefab entity data
        auto prefabEntityIt = prefab->entities.find(prefabEntity);
        if (prefabEntityIt == prefab->entities.end()) {
            return false;
        }

        // Find component data in prefab
        const auto& components = prefabEntityIt->second.components;
        auto componentIt = components.find(componentType);
        if (componentIt == components.end()) {
            return false;
        }

        // Deserialize component from prefab data
        m_registry.serialization().deserializeComponent(entity, componentType, componentIt->second);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to restore component from prefab: {}", e.what());
        return false;
    }
}

void PrefabInstanceManager::syncEntityWithPrefab(Entity instanceEntity, Entity prefabEntity,
    const PrefabInstance& instance) {
    try {
        const Prefab* prefab = Engine::get().assetSystem().prefabManager().getPrefab(instance.prefabHandle);
        if (!prefab) {
            return;
        }

        auto prefabEntityIt = prefab->entities.find(prefabEntity);
        if (prefabEntityIt == prefab->entities.end()) {
            return;
        }

        const auto& prefabComponents = prefabEntityIt->second.components;

        // Get overridden components for this entity
        std::unordered_set<std::string> overriddenComponents;
        auto overrideIt = instance.overriddenComponents.find(instanceEntity);
        if (overrideIt != instance.overriddenComponents.end()) {
            overriddenComponents = overrideIt->second;
        }

        // Sync only non-overridden components
        for (const auto& [componentType, componentData] : prefabComponents) {
            if (overriddenComponents.find(componentType) == overriddenComponents.end()) {
                // Component is not overridden, sync it
                m_registry.serialization().deserializeComponent(instanceEntity, componentType, componentData);
            }
        }
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to sync entity {} with prefab: {}", instanceEntity.id, e.what());
    }
}

bool PrefabInstanceManager::isValidInstance(PrefabInstanceHandle instanceHandle) const {
    return m_instances.find(instanceHandle.id) != m_instances.end();
}

const PrefabInstance* PrefabInstanceManager::getInstance(PrefabInstanceHandle instanceHandle) const {
    auto it = m_instances.find(instanceHandle.id);
    return it != m_instances.end() ? &it->second : nullptr;
}

PrefabInstanceHandle PrefabInstanceManager::findInstanceByEntity(Entity entity) const {
    auto it = m_entityToInstance.find(entity);
    return it != m_entityToInstance.end() ? it->second : PrefabInstanceHandle{};
}

std::vector<PrefabInstanceHandle> PrefabInstanceManager::getAllInstances() const {
    std::vector<PrefabInstanceHandle> result;
    result.reserve(m_instances.size());

    for (const auto& [id, instance] : m_instances) {
        result.push_back(instance.handle);
    }

    return result;
}

bool PrefabInstanceManager::isEntityPartOfInstance(Entity entity) const {
    return m_entityToInstance.find(entity) != m_entityToInstance.end();
}

bool PrefabInstanceManager::canDestroyEntity(Entity entity) const {
    return !isEntityPartOfInstance(entity);
}

PrefabInstanceHandle PrefabInstanceManager::getInstanceForEntity(Entity entity) const {
    return findInstanceByEntity(entity);
}

std::string PrefabInstanceManager::getInstanceName(PrefabInstanceHandle instanceHandle) const {
    auto it = m_instances.find(instanceHandle.id);
    return it != m_instances.end() ? it->second.instanceName : "";
}

void PrefabInstanceManager::setInstanceName(PrefabInstanceHandle instanceHandle, const std::string& name) {
    auto it = m_instances.find(instanceHandle.id);
    if (it != m_instances.end()) {
        it->second.instanceName = name;
    }
}

void PrefabInstanceManager::onEntityDestroyed(Entity entity) {
    auto it = m_entityToInstance.find(entity);
    if (it != m_entityToInstance.end()) {
        PrefabInstanceHandle instanceHandle = it->second;

        // Remove entity from instance
        auto instanceIt = m_instances.find(instanceHandle.id);
        if (instanceIt != m_instances.end()) {
            instanceIt->second.entities.erase(entity);
            instanceIt->second.overriddenComponents.erase(entity);
        }

        // Remove from lookup
        m_entityToInstance.erase(it);
    }
}

// Rest of implementation remains the same...
PrefabHandle PrefabInstanceManager::createPrefabFromEntity(Entity rootEntity) {
    try {
        PrefabManager& prefabManager = Engine::get().assetSystem().prefabManager();

        if (!m_registry.valid(rootEntity)) {
            SPDLOG_ERROR("Invalid root entity for prefab creation");
            return PrefabHandle{};
        }

        // Build prefab data structure
        Prefab prefabData;
        prefabData.rootEntity = rootEntity;
        prefabData.entityCount = m_registry.countEntitiesInHierarchy(rootEntity);

        // Collect all entities in hierarchy
        std::unordered_set<Entity> allEntities;
        collectEntitiesInHierarchy(rootEntity, allEntities);

        // Build prefab entity data
        for (Entity entity : allEntities) {
            PrefabEntity entityData;
            entityData.name = m_registry.getEntityName(entity);

            // Collect children
            const auto& children = m_registry.getChildren(entity);
            entityData.children.assign(children.begin(), children.end());

            // Serialize all components
            std::vector<std::string> componentTypes = m_registry.getEntityComponentTypes(entity);
            for (const std::string& componentType : componentTypes) {
                entityData.components[componentType] = m_registry.serialization().serializeComponent(entity, componentType);
            }

            prefabData.entities[entity] = std::move(entityData);
        }

        // Use entity name as filename
        std::string filename = m_registry.getEntityName(rootEntity);
        if (filename.empty()) {
            filename = "Entity_" + std::to_string(rootEntity.id);
        }

        // Create prefab
        PrefabHandle handle = prefabManager.createPrefab(prefabData, filename);

        SPDLOG_INFO("Created prefab from entity: {}", filename);
        return handle;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to create prefab from entity: {}", e.what());
        return PrefabHandle{};
    }
}

// Private helper methods remain the same...
PrefabInstanceHandle PrefabInstanceManager::generateHandle() {
    return PrefabInstanceHandle(m_nextInstanceId++);
}

void PrefabInstanceManager::collectEntitiesInHierarchy(Entity entity, std::unordered_set<Entity>& entities) {
    entities.insert(entity);

    const auto& children = m_registry.getChildren(entity);
    for (Entity child : children) {
        collectEntitiesInHierarchy(child, entities);
    }
}

Entity PrefabInstanceManager::instantiatePrefabInternal(PrefabHandle prefabHandle, Entity parent) {
    try {
        // Get prefab from PrefabManager
        const Prefab* prefab = Engine::get().assetSystem().prefabManager().getPrefab(prefabHandle);
        if (!prefab) {
            SPDLOG_ERROR("Prefab not found for handle: {}", prefabHandle.id);
            return Entity(0);
        }

        // Map old entity IDs to new ones
        std::unordered_map<Entity, Entity> entityMapping;

        // Create all entities first
        for (const auto& [oldEntity, entityData] : prefab->entities) {
            Entity newEntity = m_registry.create(Entity(0), entityData.name);
            entityMapping[oldEntity] = newEntity;
        }

        // Set up hierarchy and components
        for (const auto& [oldEntity, entityData] : prefab->entities) {
            Entity newEntity = entityMapping[oldEntity];

            // Set parent if this is not the root entity
            if (oldEntity != prefab->rootEntity) {
                // Find parent in the prefab data
                for (const auto& [potentialParent, parentData] : prefab->entities) {
                    auto it = std::find(parentData.children.begin(), parentData.children.end(), oldEntity);
                    if (it != parentData.children.end()) {
                        Entity newParent = entityMapping[potentialParent];
                        m_registry.setParent(newEntity, newParent);
                        break;
                    }
                }
            }
            else if (parent.id != 0) {
                // This is the root entity and we have a specified parent
                m_registry.setParent(newEntity, parent);
            }

            // Deserialize components
            for (const auto& [componentType, componentData] : entityData.components) {
                m_registry.serialization().deserializeComponent(newEntity, componentType, componentData);
            }
        }

        Entity newRootEntity = entityMapping[prefab->rootEntity];
        SPDLOG_DEBUG("Successfully instantiated prefab {} as entity {}", prefabHandle.id, newRootEntity.id);
        return newRootEntity;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during prefab instantiation: {}", e.what());
        return Entity(0);
    }
}

void PrefabInstanceManager::registerInstanceEntities(PrefabInstanceHandle instanceHandle, Entity rootEntity) {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return;
    }

    PrefabInstance& instance = it->second;

    // Collect all entities in the hierarchy
    collectEntitiesInHierarchy(rootEntity, instance.entities);

    // Update lookup table
    for (Entity entity : instance.entities) {
        m_entityToInstance[entity] = instanceHandle;
    }
}

void PrefabInstanceManager::unregisterInstanceEntities(PrefabInstanceHandle instanceHandle) {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return;
    }

    const PrefabInstance& instance = it->second;

    // Remove from lookup table
    for (Entity entity : instance.entities) {
        m_entityToInstance.erase(entity);
    }
}