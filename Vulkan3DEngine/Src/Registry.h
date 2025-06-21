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
#include "PrefabInstanceManager.h"
#include "SceneRegistry.h"
#include "AssetSystem.h"

// Forward declarations
class SystemManager;
class EntityManager;
class ComponentManager;
class PrefabInstanceManager;
class SceneRegistry;
class AssetSystem;
class Engine;

class Registry {
public:
    Registry(Engine& engine);

    // === DIRECT MODULE ACCESS ===

    SystemManager& systems() { return *m_systemManager; }
    EntityManager& entities() { return *m_entityManager; }
    ComponentManager& components() { return *m_componentManager; }
    PrefabInstanceManager& prefabs() { return *m_prefabInstanceManager; }
    SceneRegistry& scenes() { return *m_sceneRegistry; }

private:
    // Modules - Registry coordinates between them
    std::unique_ptr<SystemManager> m_systemManager;
    std::unique_ptr<EntityManager> m_entityManager;
    std::unique_ptr<ComponentManager> m_componentManager;
    std::unique_ptr<PrefabInstanceManager> m_prefabInstanceManager;
    std::unique_ptr<SceneRegistry> m_sceneRegistry;
};