#include "PrefabInstanceManager.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include "PrefabManager.h"
#include <spdlog/spdlog.h>

PrefabInstanceManager::PrefabInstanceManager(EntityManager& entityManager, ComponentManager& componentManager,
    PrefabManager& prefabManager)
    : m_entityManager(entityManager), m_componentManager(componentManager),
    m_prefabManager(prefabManager) {

    // Subskrybuj event niszczenia entity
    m_entityDestroyedSubscription = m_entityManager.onEntityDestroyed().subscribe(
        [this](Entity entity) {
            onEntityDestroyed(entity);
        });
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

        // NAJPIERW dodaj instancję do mapy
        m_instances[instanceHandle.id] = std::move(instance);

        // POTEM zarejestruj encje (teraz znajdzie instancję)
        registerInstanceEntities(instanceHandle, rootEntity);

        SPDLOG_DEBUG("Created prefab instance: {} with root entity: {}",
            m_instances[instanceHandle.id].instanceName, rootEntity.id);

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

    // Odblokuj wszystkie entities przed usunięciem
    for (Entity entity : instance.entities) {
        if (m_entityManager.valid(entity)) {
            m_entityManager.unlockEntity(entity);
        }
    }

    // Destroy all entities in the instance (starting from leaves)
    std::vector<Entity> entitiesToDestroy(instance.entities.begin(), instance.entities.end());

    // Sort by hierarchy depth (children first)
    std::sort(entitiesToDestroy.begin(), entitiesToDestroy.end(),
        [this](Entity a, Entity b) {
            return m_entityManager.getChildren(a).empty() && !m_entityManager.getChildren(b).empty();
        });

    for (Entity entity : entitiesToDestroy) {
        if (m_entityManager.valid(entity)) {
            m_entityManager.destroy(entity);
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

    const PrefabInstance& instance = it->second;

    // Odblokuj wszystkie entities
    for (Entity entity : instance.entities) {
        if (m_entityManager.valid(entity)) {
            m_entityManager.unlockEntity(entity);
        }
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
    if (!m_componentManager.isComponentRegistered(componentType)) {
        SPDLOG_WARN("Component type {} is not registered", componentType);
        return false;
    }

    // Check if entity has this component using serialization system
    auto componentTypes = m_componentManager.getEntityComponentTypes(entity);
    if (std::find(componentTypes.begin(), componentTypes.end(), componentType) == componentTypes.end()) {
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
    std::vector<std::string> componentTypes = m_componentManager.getEntityComponentTypes(entity);

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

void PrefabInstanceManager::destroyAllPrefabInstances()
{
    auto allInstances = getAllInstances();
    for (auto instanceHandle : allInstances) {
        destroyInstance(instanceHandle);
    }
}

bool PrefabInstanceManager::syncWithPrefab(PrefabInstanceHandle instanceHandle) {
    auto it = m_instances.find(instanceHandle.id);
    if (it == m_instances.end()) {
        return false;
    }

    PrefabInstance& instance = it->second;
    const Prefab* prefab = m_prefabManager.getPrefab(instance.prefabHandle);
    if (!prefab) {
        SPDLOG_ERROR("Prefab not found for sync operation");
        return false;
    }

    // Przekaż prefab do buildEntityMapping
    std::unordered_map<Entity, Entity> instanceToPrefabMapping;
    buildEntityMapping(instance.rootEntity, prefab->rootEntity, instanceToPrefabMapping, *prefab);

    // Sync każdej entity
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
        const Prefab* prefab = m_prefabManager.getPrefab(prefabHandle);
        if (!prefab) {
            return false;
        }

        PrefabInstanceHandle instanceHandle = findInstanceByEntity(entity);
        if (instanceHandle.id == 0) {
            return false;
        }

        const PrefabInstance* instance = getInstance(instanceHandle);
        if (!instance) {
            return false;
        }

        // Przekaż prefab do buildEntityMapping
        std::unordered_map<Entity, Entity> instanceToPrefabMapping;
        buildEntityMapping(instance->rootEntity, prefab->rootEntity, instanceToPrefabMapping, *prefab);

        auto mappingIt = instanceToPrefabMapping.find(entity);
        if (mappingIt == instanceToPrefabMapping.end()) {
            return false;
        }

        Entity prefabEntity = mappingIt->second;
        auto prefabEntityIt = prefab->entities.find(prefabEntity);
        if (prefabEntityIt == prefab->entities.end()) {
            return false;
        }

        const auto& components = prefabEntityIt->second.components;
        auto componentIt = components.find(componentType);
        if (componentIt == components.end()) {
            return false;
        }

        m_componentManager.deserializeComponent(entity, componentType, componentIt->second);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to restore component from prefab: {}", e.what());
        return false;
    }
}

void PrefabInstanceManager::buildEntityMapping(Entity instanceRoot, Entity prefabRoot,
    std::unordered_map<Entity, Entity>& mapping, const Prefab& prefab) {
    mapping[instanceRoot] = prefabRoot;

    // Pobierz dzieci z DANYCH PREFABA, nie z EntityManager
    const auto& instanceChildren = m_entityManager.getChildren(instanceRoot);

    // Znajdź dane prefab entity
    auto prefabEntityIt = prefab.entities.find(prefabRoot);
    if (prefabEntityIt == prefab.entities.end()) {
        SPDLOG_ERROR("Prefab entity {} not found in prefab data", prefabRoot.id);
        return;
    }

    const auto& prefabChildren = prefabEntityIt->second.children;  // ✅ Pobierz z danych prefaba!

    // Lepsze mapowanie - na podstawie nazw lub pozycji w hierarchii
    if (instanceChildren.size() != prefabChildren.size()) {
        SPDLOG_WARN("Instance and prefab have different number of children ({} vs {})",
            instanceChildren.size(), prefabChildren.size());
    }

    // Konwertuj unordered_set na vector dla stabilnej kolejności
    std::vector<Entity> instanceChildrenVec(instanceChildren.begin(), instanceChildren.end());
    std::vector<Entity> prefabChildrenVec(prefabChildren.begin(), prefabChildren.end());

    // Sortuj po ID dla przewidywalnej kolejności (lub użyj nazw)
    std::sort(instanceChildrenVec.begin(), instanceChildrenVec.end(),
        [](Entity a, Entity b) { return a.id < b.id; });

    size_t minSize = std::min(instanceChildrenVec.size(), prefabChildrenVec.size());
    for (size_t i = 0; i < minSize; ++i) {
        buildEntityMapping(instanceChildrenVec[i], prefabChildrenVec[i], mapping, prefab);
    }
}

void PrefabInstanceManager::syncEntityWithPrefab(Entity instanceEntity, Entity prefabEntity,
    const PrefabInstance& instance) {
    try {
        const Prefab* prefab = m_prefabManager.getPrefab(instance.prefabHandle);
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
                m_componentManager.deserializeComponent(instanceEntity, componentType, componentData);
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

bool PrefabInstanceManager::isInstanceRoot(Entity entity) const {
    auto it = m_entityToInstance.find(entity);
    if (it == m_entityToInstance.end()) {
        return false;
    }

    auto instanceIt = m_instances.find(it->second.id);
    if (instanceIt == m_instances.end()) {
        return false;
    }

    return instanceIt->second.rootEntity == entity;
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

        // Sprawdź czy to root entity instancji
        auto instanceIt = m_instances.find(instanceHandle.id);
        if (instanceIt != m_instances.end() && instanceIt->second.rootEntity == entity) {
            SPDLOG_INFO("Root entity {} destroyed, removing entire prefab instance {}",
                entity.id, instanceHandle.id);

            // Usuń całą instancję
            destroyInstance(instanceHandle);
        }
        else {
            // Zwykłe entity - usuń tylko z instancji
            if (instanceIt != m_instances.end()) {
                instanceIt->second.entities.erase(entity);
                instanceIt->second.overriddenComponents.erase(entity);
            }
            m_entityToInstance.erase(it);
        }
    }
}

PrefabHandle PrefabInstanceManager::createPrefabFromEntity(Entity rootEntity) {
    try {
        if (!m_entityManager.valid(rootEntity)) {
            SPDLOG_ERROR("Invalid root entity for prefab creation");
            return PrefabHandle{};
        }

        // Build prefab data structure
        Prefab prefabData;
        prefabData.rootEntity = rootEntity;
        prefabData.entityCount = m_entityManager.countEntitiesInHierarchy(rootEntity);

        // Collect all entities in hierarchy
        std::unordered_set<Entity> allEntities;
        collectEntitiesInHierarchy(rootEntity, allEntities);

        // Build prefab entity data with proper hierarchy
        for (Entity entity : allEntities) {
            PrefabEntity entityData;
            entityData.name = m_entityManager.getEntityName(entity);

            // Collect ONLY direct children (not all descendants)
            const auto& children = m_entityManager.getChildren(entity);
            entityData.children.assign(children.begin(), children.end());

            // Store parent information for non-root entities
            if (entity != rootEntity && m_entityManager.hasParent(entity)) {
                entityData.parent = m_entityManager.getParent(entity);
            }
            else {
                entityData.parent = Entity(0); // Explicit default for root entity
            }

            // Serialize all components
            std::vector<std::string> componentTypes = m_componentManager.getEntityComponentTypes(entity);
            for (const std::string& componentType : componentTypes) {
                entityData.components[componentType] = m_componentManager.serializeComponent(entity, componentType);
            }

            prefabData.entities[entity] = std::move(entityData);
        }

        // Use entity name as filename
        std::string filename = m_entityManager.getEntityName(rootEntity);
        if (filename.empty()) {
            filename = "Entity_" + std::to_string(rootEntity.id);
        }

        // Create prefab
        PrefabHandle handle = m_prefabManager.createPrefab(prefabData, filename);

        SPDLOG_INFO("Created prefab from entity: {}", filename);
        return handle;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to create prefab from entity: {}", e.what());
        return PrefabHandle{};
    }
}

PrefabInstanceHandle PrefabInstanceManager::generateHandle() {
    return PrefabInstanceHandle(m_nextInstanceId++);
}

void PrefabInstanceManager::collectEntitiesInHierarchy(Entity entity, std::unordered_set<Entity>& entities) {
    entities.insert(entity);

    const auto& children = m_entityManager.getChildren(entity);
    for (Entity child : children) {
        collectEntitiesInHierarchy(child, entities);
    }
}

Entity PrefabInstanceManager::instantiatePrefabInternal(PrefabHandle prefabHandle, Entity parent) {
    try {
        // Get prefab from PrefabManager
        const Prefab* prefab = m_prefabManager.getPrefab(prefabHandle);
        if (!prefab) {
            SPDLOG_ERROR("Prefab not found for handle: {}", prefabHandle.id);
            return Entity(0);
        }

        // Map old entity IDs to new ones
        std::unordered_map<Entity, Entity> entityMapping;

        // Phase 1: Create all entities first (without hierarchy)
        for (const auto& [oldEntity, entityData] : prefab->entities) {
            Entity newEntity = m_entityManager.create(entityData.name);
            entityMapping[oldEntity] = newEntity;
            SPDLOG_DEBUG("Mapped prefab entity {} to new entity {}", oldEntity.id, newEntity.id);
        }

        // Phase 2: Set up hierarchy ONLY using parent information to avoid duplicates
        Entity newRootEntity = entityMapping[prefab->rootEntity];
        
        // First, set the root entity's parent if provided
        if (parent.id != 0 && m_entityManager.valid(parent)) {
            m_entityManager.setParent(newRootEntity, parent);
            SPDLOG_DEBUG("Set parent {} for root entity {}", parent.id, newRootEntity.id);
        }

        // Then, set up all other parent-child relationships
        for (const auto& [oldEntity, entityData] : prefab->entities) {
            // Skip root entity (already handled above)
            if (oldEntity == prefab->rootEntity) {
                continue;
            }

            Entity newEntity = entityMapping[oldEntity];

            // Only process entities that have a parent in the prefab
            if (entityData.parent.id != 0) {
                auto parentMappingIt = entityMapping.find(entityData.parent);
                if (parentMappingIt != entityMapping.end()) {
                    Entity newParent = parentMappingIt->second;
                    if (m_entityManager.valid(newParent)) {
                        m_entityManager.setParent(newEntity, newParent);
                        SPDLOG_DEBUG("Set parent {} for entity {}", newParent.id, newEntity.id);
                    }
                    else {
                        SPDLOG_WARN("Mapped parent entity {} is not valid for entity {}",
                            newParent.id, newEntity.id);
                    }
                }
                else {
                    SPDLOG_WARN("Could not find mapping for parent entity {} of entity {}",
                        entityData.parent.id, oldEntity.id);
                }
            }
        }

        // Phase 3: Deserialize components after hierarchy is set up
        for (const auto& [oldEntity, entityData] : prefab->entities) {
            Entity newEntity = entityMapping[oldEntity];

            // Deserialize components
            for (const auto& [componentType, componentData] : entityData.components) {
                try {
                    m_componentManager.deserializeComponent(newEntity, componentType, componentData);
                }
                catch (const std::exception& e) {
                    SPDLOG_ERROR("Failed to deserialize component {} for entity {}: {}",
                        componentType, newEntity.id, e.what());
                }
            }
        }

        // Lock all entities in the instance to prevent accidental modification
        for (const auto& [oldEntity, entityData] : prefab->entities) {
            Entity newEntity = entityMapping[oldEntity];
            m_entityManager.lockEntity(newEntity);
        }

        // Verify hierarchy was created correctly
        if (!m_entityManager.valid(newRootEntity)) {
            SPDLOG_ERROR("Root entity became invalid during instantiation");
            return Entity(0);
        }

        // Debug: Log final hierarchy structure
        SPDLOG_DEBUG("Successfully instantiated prefab {} as entity {} with {} children", 
            prefabHandle.id, newRootEntity.id, m_entityManager.getChildren(newRootEntity).size());
        
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