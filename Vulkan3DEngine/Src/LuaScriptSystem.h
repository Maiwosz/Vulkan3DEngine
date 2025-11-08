#pragma once
#include "IScriptSystem.h"
#include "ScriptComponent.h"
#include <sol/sol.hpp>
#include <unordered_map>
#include <memory>
#include <set>

class Engine;
class Registry;

class LuaScriptSystem : public IScriptSystem {
public:
    LuaScriptSystem() = default;
    ~LuaScriptSystem();

    void initialize() override;
    void update(float deltaTime) override;
    void shutdown() override;

    bool initializeScript(Entity entity) override;
    void updateScript(Entity entity, float deltaTime) override;
    void destroyScript(Entity entity) override;

    bool isScriptInitialized(Entity entity) const override;
    std::string getScriptSystemName() const override { return "LuaScriptSystem"; }

    // Lua-specific methods
    sol::state* getLuaState() { return m_luaState.get(); }
    lua_State* getRawLuaState() {
        return m_luaState ? m_luaState->lua_state() : nullptr;
    }

    // Set references (called by ScriptSystem)
    void setEngine(Engine* engine) { m_engine = engine; }
    void setRegistry(Registry* registry) { m_registry = registry; }

private:
    bool loadScript(Entity entity, ScriptComponent& script);

    void callOnCreate(Entity entity, sol::table& scriptTable);
    void callOnUpdate(Entity entity, sol::table& scriptTable, float deltaTime);
    void callOnDestroy(Entity entity, sol::table& scriptTable);

    std::set<Entity> m_createdScripts;
    std::unordered_map<Entity, sol::table> m_scriptInstances;
    std::unique_ptr<sol::state> m_luaState;

    Engine* m_engine = nullptr;
    Registry* m_registry = nullptr;
};
