#include "Registry.h"
#include "SystemManager.h"
#include "Engine.h"
#include <stdexcept>
#include <algorithm>
#include <spdlog/spdlog.h>

Registry::Registry(Engine& engine) {
    // Create modules in CORRECT dependency order
    // EntityManager and ComponentManager first (they don't depend on systems)
    m_componentManager = std::make_unique<ComponentManager>(engine, *this);
    m_entityManager = std::make_unique<EntityManager>(*m_componentManager);

    // Then create managers that depend on entities/components
    m_prefabInstanceManager = std::make_unique<PrefabInstanceManager>(
        *m_entityManager,
        *m_componentManager,
        engine.assetSystem().prefabManager()
    );

    m_sceneRegistry = std::make_unique<SceneRegistry>(
        *m_entityManager,
        *m_componentManager,
        *m_prefabInstanceManager,
        engine.assetSystem().sceneManager()
    );

    // SystemManager last (it may immediately start using entities/components)
    m_systemManager = std::make_unique<SystemManager>(engine, *this);
}