#include "CppScriptSystem.h"
#include "Engine.h"
#include "Registry.h"
#include "ComponentManager.h"
#include "CppScriptRegistry.h"  // Tylko dla registerAllCppScripts
#include <spdlog/spdlog.h>

CppScriptSystem::~CppScriptSystem() {
    shutdown();
}

void CppScriptSystem::initialize() {
    SPDLOG_DEBUG("Initializing CppScriptSystem");

    // Now we can safely register scripts since registry is available
    registerAllScripts();

    SPDLOG_INFO("CppScriptSystem initialized with {} registered script types",
        m_scriptTypes.size());
}

void CppScriptSystem::registerAllScripts() {
    if (!m_registry) {
        SPDLOG_ERROR("Cannot register scripts - registry not set");
        return;
    }

    // Call the central registration function from CppScriptRegistry.h
    registerAllCppScripts(*this);
}

void CppScriptSystem::update(float deltaTime) {
    if (!m_registry) return;

    // Iterate through all registered script types
    for (const auto& [type, name] : m_scriptTypes) {
        // Get component pool for this script type
        auto* pool = m_registry->components().getComponentPool(name);
        if (!pool) continue;

        auto entities = pool->getOrderedEntities();

        // First pass: Initialize scripts
        for (auto entity : entities) {
            auto* scriptComponent = dynamic_cast<CppScriptBase*>(
                m_registry->components().getComponentByName(entity, name));

            if (!scriptComponent) continue;

            auto& scriptSet = m_initializedScripts[entity];
            bool wasInitialized = scriptSet.count(type) > 0;

            if (!scriptComponent->isInitialized() && !wasInitialized) {
                SPDLOG_DEBUG("Initializing C++ script {} for entity {}",
                    name, entity.id);

                scriptComponent->setInitialized(true);
                scriptSet.insert(type);
                m_createdScripts.insert(entity);
            }
        }

        // Second pass: Call OnCreate for newly initialized scripts
        for (auto entity : m_createdScripts) {
            if (!m_registry->entities().valid(entity)) continue;

            auto* scriptComponent = dynamic_cast<CppScriptBase*>(
                m_registry->components().getComponentByName(entity, name));

            if (scriptComponent && scriptComponent->isInitialized()) {
                callOnCreate(entity, scriptComponent);
            }
        }

        // Third pass: Call OnUpdate for all initialized scripts
        for (auto entity : entities) {
            if (!m_registry->entities().valid(entity)) continue;

            auto* scriptComponent = dynamic_cast<CppScriptBase*>(
                m_registry->components().getComponentByName(entity, name));

            if (scriptComponent && scriptComponent->isInitialized()) {
                callOnUpdate(entity, scriptComponent, deltaTime);
            }
        }
    }

    m_createdScripts.clear();

    // Check for destroyed entities
    std::vector<Entity> toRemove;
    for (auto& [entity, scriptSet] : m_initializedScripts) {
        if (!m_registry->entities().valid(entity)) {
            // Call OnDestroy for all scripts on this entity
            for (const auto& type : scriptSet) {
                auto it = m_scriptTypes.find(type);
                if (it != m_scriptTypes.end()) {
                    auto* scriptComponent = dynamic_cast<CppScriptBase*>(
                        m_registry->components().getComponentByName(entity, it->second));

                    if (scriptComponent) {
                        callOnDestroy(entity, scriptComponent);
                    }
                }
            }
            toRemove.push_back(entity);
        }
    }

    for (auto entity : toRemove) {
        m_initializedScripts.erase(entity);
    }
}

void CppScriptSystem::shutdown() {
    // Call OnDestroy for all initialized scripts
    /*for (auto& [entity, scriptSet] : m_initializedScripts) {
        for (const auto& type : scriptSet) {
            auto it = m_scriptTypes.find(type);
            if (it != m_scriptTypes.end() && m_registry) {
                auto* scriptComponent = dynamic_cast<CppScriptBase*>(
                    m_registry->components().getComponentByName(entity, it->second));

                if (scriptComponent) {
                    callOnDestroy(entity, scriptComponent);
                }
            }
        }
    }*/

    m_initializedScripts.clear();
    m_createdScripts.clear();
}

bool CppScriptSystem::initializeScript(Entity entity) {
    if (!m_registry) return false;

    bool anyInitialized = false;

    for (const auto& [type, name] : m_scriptTypes) {
        auto* scriptComponent = dynamic_cast<CppScriptBase*>(
            m_registry->components().getComponentByName(entity, name));

        if (scriptComponent && !scriptComponent->isInitialized()) {
            scriptComponent->setInitialized(true);
            m_initializedScripts[entity].insert(type);
            callOnCreate(entity, scriptComponent);
            anyInitialized = true;
        }
    }

    return anyInitialized;
}

void CppScriptSystem::updateScript(Entity entity, float deltaTime) {
    if (!m_registry) return;

    for (const auto& [type, name] : m_scriptTypes) {
        auto* scriptComponent = dynamic_cast<CppScriptBase*>(
            m_registry->components().getComponentByName(entity, name));

        if (scriptComponent && scriptComponent->isInitialized()) {
            callOnUpdate(entity, scriptComponent, deltaTime);
        }
    }
}

void CppScriptSystem::destroyScript(Entity entity) {
    if (!m_registry) return;

    auto it = m_initializedScripts.find(entity);
    if (it == m_initializedScripts.end()) return;

    for (const auto& type : it->second) {
        auto typeIt = m_scriptTypes.find(type);
        if (typeIt != m_scriptTypes.end()) {
            auto* scriptComponent = dynamic_cast<CppScriptBase*>(
                m_registry->components().getComponentByName(entity, typeIt->second));

            if (scriptComponent) {
                callOnDestroy(entity, scriptComponent);
            }
        }
    }

    m_initializedScripts.erase(it);
}

bool CppScriptSystem::isScriptInitialized(Entity entity) const {
    auto it = m_initializedScripts.find(entity);
    return it != m_initializedScripts.end() && !it->second.empty();
}

bool CppScriptSystem::hasAnyCppScript(Entity entity) const {
    if (!m_registry) return false;

    for (const auto& [type, name] : m_scriptTypes) {
        if (m_registry->components().getComponentByName(entity, name)) {
            return true;
        }
    }

    return false;
}

void CppScriptSystem::callOnCreate(Entity entity, CppScriptBase* script) {
    if (!script) return;

    try {
        script->OnCreate();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in OnCreate for C++ script {} on entity {}: {}",
            script->getScriptName(), entity.id, e.what());
    }
}

void CppScriptSystem::callOnUpdate(Entity entity, CppScriptBase* script, float deltaTime) {
    if (!script) return;

    try {
        script->OnUpdate(deltaTime);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in OnUpdate for C++ script {} on entity {}: {}",
            script->getScriptName(), entity.id, e.what());
    }
}

void CppScriptSystem::callOnDestroy(Entity entity, CppScriptBase* script) {
    if (!script) return;

    try {
        script->OnDestroy();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in OnDestroy for C++ script {} on entity {}: {}",
            script->getScriptName(), entity.id, e.what());
    }
}
