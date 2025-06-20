#include "LuaBindingsEngine.h"
#include "Engine.h"
#include "Scene.h"

namespace LuaBindings {
    void registerEngineFunctions(sol::state& state, Engine& engine, Registry& registry) {
        // Create Engine namespace in Lua
        auto engineNS = state.create_named_table("Engine");

        // Register core engine functions
        engineNS.set_function("getRegistry", [&registry]() -> Registry& {
            return registry;
            });

        engineNS.set_function("getDeltaTime", [&engine]() -> float {
            return engine.deltaTime();
            });

        engineNS.set_function("getTotalTime", [&engine]() -> float {
            return engine.totalTime();
            });

    }
}