#include "LuaScriptSystem.h"
#include "Engine.h"
#include "Registry.h"
#include "ComponentManager.h"
#include "LuaBindings.h"
#include "Paths.h"
#include <spdlog/spdlog.h>
#include <filesystem>

LuaScriptSystem::~LuaScriptSystem() {
    shutdown();
}

void LuaScriptSystem::initialize() {
    SPDLOG_DEBUG("Initializing LuaScriptSystem");

    m_luaState = std::make_unique<sol::state>();

    // Open Lua libraries
    m_luaState->open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table,
        sol::lib::io,
        sol::lib::os
    );

    // Register Lua bindings
    LuaBindings::registerAll(*m_luaState, *m_engine, *m_registry);

    SPDLOG_INFO("LuaScriptSystem initialized");
}

void LuaScriptSystem::update(float deltaTime) {
    if (!m_registry) return;

    auto entities = m_registry->components().createOrderedView<ScriptComponent>();

    if (entities.empty()) {
        return;
    }

    // First pass: Initialize scripts
    for (auto entity : entities) {
        auto& script = m_registry->components().getComponent<ScriptComponent>(entity);

        if (!script.isInitialized()) {
            SPDLOG_DEBUG("Initializing Lua script for entity {}, path: {}",
                entity.id, script.getScript());

            if (loadScript(entity, script)) {
                script.setInitialized(true);
                m_createdScripts.insert(entity);
            }
            else {
                SPDLOG_ERROR("Failed to initialize Lua script for entity {}", entity.id);
            }
        }
    }

    // Second pass: Call OnCreate
    for (auto entity : m_createdScripts) {
        if (m_registry->entities().valid(entity) &&
            m_registry->components().hasComponent<ScriptComponent>(entity)) {
            auto scriptIt = m_scriptInstances.find(entity);
            if (scriptIt != m_scriptInstances.end()) {
                callOnCreate(entity, scriptIt->second);
            }
        }
    }
    m_createdScripts.clear();

    // Third pass: Call OnUpdate
    for (auto entity : entities) {
        if (m_registry->entities().valid(entity)) {
            auto scriptIt = m_scriptInstances.find(entity);
            if (scriptIt != m_scriptInstances.end()) {
                callOnUpdate(entity, scriptIt->second, deltaTime);
            }
        }
    }

    // Check for destroyed entities
    std::vector<Entity> toRemove;
    for (auto& [entity, scriptTable] : m_scriptInstances) {
        if (!m_registry->entities().valid(entity)) {
            callOnDestroy(entity, scriptTable);
            toRemove.push_back(entity);
        }
    }

    for (auto entity : toRemove) {
        m_scriptInstances.erase(entity);
    }
}

void LuaScriptSystem::shutdown() {
    // Call OnDestroy for all scripts
  /*  for (auto& [entity, scriptTable] : m_scriptInstances) {
        callOnDestroy(entity, scriptTable);
    }*/
    m_scriptInstances.clear();
    m_luaState.reset();
}

bool LuaScriptSystem::initializeScript(Entity entity) {
    if (!m_registry || !m_registry->components().hasComponent<ScriptComponent>(entity)) {
        return false;
    }

    auto& script = m_registry->components().getComponent<ScriptComponent>(entity);
    return loadScript(entity, script);
}

void LuaScriptSystem::updateScript(Entity entity, float deltaTime) {
    auto scriptIt = m_scriptInstances.find(entity);
    if (scriptIt != m_scriptInstances.end()) {
        callOnUpdate(entity, scriptIt->second, deltaTime);
    }
}

void LuaScriptSystem::destroyScript(Entity entity) {
    auto scriptIt = m_scriptInstances.find(entity);
    if (scriptIt != m_scriptInstances.end()) {
        callOnDestroy(entity, scriptIt->second);
        m_scriptInstances.erase(scriptIt);
    }
}

bool LuaScriptSystem::isScriptInitialized(Entity entity) const {
    return m_scriptInstances.find(entity) != m_scriptInstances.end();
}

bool LuaScriptSystem::loadScript(Entity entity, ScriptComponent& script) {
    const std::string& path = script.getScript();
    if (path.empty()) {
        SPDLOG_ERROR("Script path is empty for entity {}", entity.id);
        return false;
    }

    std::string scriptPath = path;
    if (scriptPath.find('.') == std::string::npos) {
        scriptPath += ".lua";
    }

    std::vector<std::string> pathsToTry;
    if (std::filesystem::path(scriptPath).is_absolute()) {
        pathsToTry.push_back(scriptPath);
    }
    else {
        pathsToTry.push_back(SCRIPTS_DIR + scriptPath);
        pathsToTry.push_back(std::filesystem::current_path().string() + "/" + scriptPath);
        pathsToTry.push_back(scriptPath);
    }

    std::string absolutePath;
    bool found = false;

    for (const auto& tryPath : pathsToTry) {
        if (std::filesystem::exists(tryPath)) {
            absolutePath = tryPath;
            found = true;
            SPDLOG_DEBUG("Found Lua script at: {}", absolutePath);
            break;
        }
    }

    if (!found) {
        SPDLOG_ERROR("Lua script not found: {}", path);
        return false;
    }

    try {
        // Clean up existing script instance
        auto existingIt = m_scriptInstances.find(entity);
        if (existingIt != m_scriptInstances.end()) {
            callOnDestroy(entity, existingIt->second);
            m_scriptInstances.erase(existingIt);
        }

        // Create script table
        sol::table scriptTable = m_luaState->create_table();
        scriptTable["entity"] = entity;
        scriptTable["self"] = scriptTable;

        // Create environment
        sol::environment env = sol::environment(*m_luaState, sol::create, m_luaState->globals());
        env["self"] = scriptTable;
        env["registry"] = m_registry;

        // Execute script
        SPDLOG_DEBUG("Loading Lua script: {}", absolutePath);
        auto result = m_luaState->script_file(absolutePath, env,
            [entity](lua_State*, sol::protected_function_result pfr) {
                if (!pfr.valid()) {
                    sol::error err = pfr;
                    SPDLOG_ERROR("Lua error for entity {}: {}", entity.id, err.what());
                }
                return pfr;
            });

        if (!result.valid()) {
            SPDLOG_ERROR("Failed to load Lua script: {}", absolutePath);
            return false;
        }

        // Store functions
        if (env["OnCreate"].valid()) scriptTable["OnCreate"] = env["OnCreate"];
        if (env["OnUpdate"].valid()) scriptTable["OnUpdate"] = env["OnUpdate"];
        if (env["OnDestroy"].valid()) scriptTable["OnDestroy"] = env["OnDestroy"];

        m_scriptInstances[entity] = scriptTable;
        script.setTable(&m_scriptInstances[entity]);
        script.setLuaState(m_luaState.get());

        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception loading Lua script {} for entity {}: {}",
            absolutePath, entity.id, e.what());
        return false;
    }
}

void LuaScriptSystem::callOnCreate(Entity entity, sol::table& scriptTable) {
    if (scriptTable["OnCreate"].valid()) {
        try {
            sol::protected_function onCreateFunc = scriptTable["OnCreate"];
            auto result = onCreateFunc(scriptTable);
            if (!result.valid()) {
                sol::error err = result;
                SPDLOG_ERROR("Error in OnCreate for entity {}: {}", entity.id, err.what());
            }
        }
        catch (const sol::error& e) {
            SPDLOG_ERROR("Exception in OnCreate for entity {}: {}", entity.id, e.what());
        }
    }
}

void LuaScriptSystem::callOnUpdate(Entity entity, sol::table& scriptTable, float deltaTime) {
    if (scriptTable["OnUpdate"].valid()) {
        try {
            sol::protected_function onUpdateFunc = scriptTable["OnUpdate"];
            auto result = onUpdateFunc(scriptTable, deltaTime);
            if (!result.valid()) {
                sol::error err = result;
                SPDLOG_ERROR("Error in OnUpdate for entity {}: {}", entity.id, err.what());
            }
        }
        catch (const sol::error& e) {
            SPDLOG_ERROR("Exception in OnUpdate for entity {}: {}", entity.id, e.what());
        }
    }
}

void LuaScriptSystem::callOnDestroy(Entity entity, sol::table& scriptTable) {
    if (scriptTable["OnDestroy"].valid()) {
        try {
            sol::protected_function onDestroyFunc = scriptTable["OnDestroy"];
            auto result = onDestroyFunc(scriptTable);
            if (!result.valid()) {
                sol::error err = result;
                SPDLOG_ERROR("Error in OnDestroy for entity {}: {}", entity.id, err.what());
            }
        }
        catch (const sol::error& e) {
            SPDLOG_ERROR("Exception in OnDestroy for entity {}: {}", entity.id, e.what());
        }
    }
}
