#include "ScriptSystem.h"
#include "Engine.h"
#include "LuaBindings.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <spdlog/spdlog.h>
#include "Paths.h"
#include "Scene.h"

void ScriptSystem::initialize() {
    SPDLOG_DEBUG("Initializing ScriptSystem");

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

    // Register bindings using the LuaBindings namespace
    LuaBindings::registerAll(*m_luaState, *m_engine, *m_registry);

    SPDLOG_DEBUG("Current working directory: {}",
        std::filesystem::current_path().string());

    SPDLOG_INFO("ScriptSystem initialized");
}

ScriptSystem::~ScriptSystem() {
    // Call OnDestroy for all scripts
    for (auto& [entity, scriptTable] : m_scriptInstances) {
        callOnDestroy(entity, scriptTable);
    }
    m_scriptInstances.clear();
}

void ScriptSystem::update() {
    float deltaTime = engine().deltaTime();

    // Get all entities with a ScriptComponent
    auto entities = registry().createView<ScriptComponent>();

    if (entities.empty()) {
        // No script components found - log only once every few seconds
        static float lastLogTime = 0.0f;
        float currentTime = engine().totalTime();
        if (currentTime - lastLogTime > 5.0f) {
            SPDLOG_DEBUG("No script components in registry");
            lastLogTime = currentTime;
        }
        return;
    }

    // First pass: Initialize scripts that haven't been initialized yet
    for (auto entity : entities) {
        auto& script = registry().components().getComponent<ScriptComponent>(entity);

        // Initialize the script if not already initialized
        if (!script.isInitialized()) {
            SPDLOG_DEBUG("Initializing script for entity {}, path: {}",
                entity.id, script.getScript());

            if (loadScript(entity, script)) {
                script.setInitialized(true);
                // Store for OnCreate call
                m_createdScripts.insert(entity);
            }
            else {
                SPDLOG_ERROR("Failed to initialize script for entity {}", entity.id);
            }
        }
    }

    // Second pass: Call OnCreate for newly initialized scripts
    for (auto entity : m_createdScripts) {
        if (registry().entities().valid(entity) && registry().components().hasComponent<ScriptComponent>(entity)) {
            auto scriptIt = m_scriptInstances.find(entity);
            if (scriptIt != m_scriptInstances.end()) {
                callOnCreate(entity, scriptIt->second);
            }
        }
    }
    m_createdScripts.clear();

    // Third pass: Call OnUpdate for all initialized scripts
    for (auto entity : entities) {
        if (registry().entities().valid(entity)) {
            auto scriptIt = m_scriptInstances.find(entity);
            if (scriptIt != m_scriptInstances.end()) {
                callOnUpdate(entity, scriptIt->second, deltaTime);
            }
        }
    }

    // Check for destroyed entities and call OnDestroy
    std::vector<Entity> toRemove;
    for (auto& [entity, scriptTable] : m_scriptInstances) {
        if (!registry().entities().valid(entity)) {
            callOnDestroy(entity, scriptTable);
            toRemove.push_back(entity);
        }
    }

    // Remove destroyed scripts
    for (auto entity : toRemove) {
        m_scriptInstances.erase(entity);
    }
}


bool ScriptSystem::loadScript(Entity entity, ScriptComponent& script) {
    const std::string& path = script.getScript();
    if (path.empty()) {
        SPDLOG_ERROR("Script path is empty for entity {}", entity.id);
        return false;
    }

    // Normalize file extension - add .lua if no extension provided
    std::string scriptPath = path;
    if (scriptPath.find('.') == std::string::npos) {
        scriptPath += ".lua"; // Assuming Lua scripts, adjust as needed
    }

    // Check if file exists - try multiple path options
    std::vector<std::string> pathsToTry;

    // Option 1: Direct absolute path as provided
    if (std::filesystem::path(scriptPath).is_absolute()) {
        pathsToTry.push_back(scriptPath);
    }
    else {
        // Option 2: Relative to SCRIPTS_DIR (default)
        pathsToTry.push_back(SCRIPTS_DIR + scriptPath);

        // Option 3: Relative to current working directory
        pathsToTry.push_back(std::filesystem::current_path().string() + "/" + scriptPath);

        // Option 4: Direct relative path as provided
        pathsToTry.push_back(scriptPath);
    }

    // Try each path option
    std::string absolutePath;
    bool found = false;

    for (const auto& tryPath : pathsToTry) {
        if (std::filesystem::exists(tryPath)) {
            absolutePath = tryPath;
            found = true;
            SPDLOG_DEBUG("Found script at: {}", absolutePath);
            break;
        }
    }

    try {
        // Clean up any existing script instance for this entity to prevent memory leaks
        auto existingIt = m_scriptInstances.find(entity);
        if (existingIt != m_scriptInstances.end()) {
            // Call OnDestroy on the old script if it exists
            callOnDestroy(entity, existingIt->second);
            // Erase the old script
            m_scriptInstances.erase(existingIt);
        }

        // Create a new table for this script instance that will be 'self'
        sol::table scriptTable = m_luaState->create_table();

        // Set entity ID in the script
        scriptTable["entity"] = entity;

        // Set the self reference - CRITICAL for the script to work properly
        scriptTable["self"] = scriptTable;

        // Create a new environment for this script instance to avoid global pollution
        // but still have access to global functions
        sol::environment env = sol::environment(*m_luaState, sol::create, m_luaState->globals());

        // Make the scriptTable available as 'self' in the script's environment
        env["self"] = scriptTable;

        // Add additional helpful globals for the script
        env["registry"] = m_registry;

        // Execute the script file in this environment
        SPDLOG_DEBUG("Loading script: {}", absolutePath);

        auto result = m_luaState->script_file(absolutePath,
            env,
            [entity](lua_State*, sol::protected_function_result pfr) {
                // Error handler
                if (!pfr.valid()) {
                    sol::error err = pfr;
                    SPDLOG_ERROR("Lua error for entity {}: {}", entity.id, err.what());
                }
                return pfr;
            });

        if (!result.valid()) {
            SPDLOG_ERROR("Failed to load script: {}", absolutePath);
            return false;
        }

        // Copy functions from the environment to the script table
        bool hasOnCreate = env["OnCreate"].valid();
        bool hasOnUpdate = env["OnUpdate"].valid();
        bool hasOnDestroy = env["OnDestroy"].valid();

        SPDLOG_DEBUG("Script for entity {} - functions available: OnCreate: {}, OnUpdate: {}, OnDestroy: {}",
            entity.id, hasOnCreate, hasOnUpdate, hasOnDestroy);

        // Store functions in the script table
        if (hasOnCreate) scriptTable["OnCreate"] = env["OnCreate"];
        if (hasOnUpdate) scriptTable["OnUpdate"] = env["OnUpdate"];
        if (hasOnDestroy) scriptTable["OnDestroy"] = env["OnDestroy"];

        // Store the script instance
        m_scriptInstances[entity] = scriptTable;

        // Set the script component's table
        script.setTable(&m_scriptInstances[entity]);
        script.setLuaState(m_luaState.get());

        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception loading script {} for entity {}: {}",
            absolutePath, entity.id, e.what());
        return false;
    }
}

void ScriptSystem::callOnCreate(Entity entity, sol::table& scriptTable) {
    // Call OnCreate if it exists
    if (scriptTable["OnCreate"].valid()) {
        try {
            // Get the function
            sol::protected_function onCreateFunc = scriptTable["OnCreate"];

            // Call function with explicit self parameter
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

void ScriptSystem::callOnUpdate(Entity entity, sol::table& scriptTable, float deltaTime) {
    // Call OnUpdate if it exists
    if (scriptTable["OnUpdate"].valid()) {
        try {
            // Get the function
            sol::protected_function onUpdateFunc = scriptTable["OnUpdate"];

            // Call function with explicit self parameter and deltaTime
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

void ScriptSystem::callOnDestroy(Entity entity, sol::table& scriptTable) {
    // Call OnDestroy if it exists
    if (scriptTable["OnDestroy"].valid()) {
        try {
            // Get the function
            sol::protected_function onDestroyFunc = scriptTable["OnDestroy"];

            // Call function with explicit self parameter
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