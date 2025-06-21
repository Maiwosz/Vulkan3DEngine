#include "Registry.h"
#include "SystemManager.h"
#include "Engine.h"
#include <stdexcept>
#include <algorithm>
#include <spdlog/spdlog.h>

Registry::Registry(Engine& engine) {
    // Create modules in dependency order
    m_systemManager = std::make_unique<SystemManager>(engine, *this);
    m_componentManager = std::make_unique<ComponentManager>(engine, *this);
    m_entityManager = std::make_unique<EntityManager>(*m_componentManager);
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
}
