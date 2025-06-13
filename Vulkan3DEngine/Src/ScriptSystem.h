#pragma once
#include "System.h"
#include "ScriptComponent.h"
#include "TransformComponent.h"
#include "Registry.h"
#include <sol/sol.hpp>
#include <unordered_map>
#include <memory>

class ScriptSystem : public System<> {
public:
    ScriptSystem();
    ~ScriptSystem();

    void update(SystemContext<>& context) override;

    // Add a getter for the Lua state to use in bindings
    sol::state* getLuaState() { return m_luaState.get(); }

    // Add a method to expose the raw Lua state pointer
    lua_State* getRawLuaState() {
        return m_luaState ? m_luaState->lua_state() : nullptr;
    }

private:
    // Load a script from file
    bool loadScript(Entity entity, ScriptComponent& script);

    // Call script lifecycle functions if they exist
    void callOnCreate(Entity entity, sol::table& scriptTable);
    void callOnUpdate(Entity entity, sol::table& scriptTable, float deltaTime);
    void callOnDestroy(Entity entity, sol::table& scriptTable);

    // Track which scripts have been created/destroyed
    std::set<Entity> m_createdScripts;
    std::unordered_map<Entity, sol::table> m_scriptInstances;

    // The Lua state
    std::unique_ptr<sol::state> m_luaState;
};