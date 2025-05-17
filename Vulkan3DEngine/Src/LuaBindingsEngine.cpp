#include "LuaBindingsEngine.h"
#include "Engine.h"
#include "Scene.h"

namespace LuaBindings {
    void registerEngineFunctions(sol::state& state) {
        // Create Engine namespace in Lua
        auto engineNS = state.create_named_table("Engine");

        // Register core engine functions
        engineNS.set_function("getRegistry", []() -> Registry& {
            return Engine::get().scene().registry();
            });

        engineNS.set_function("getDeltaTime", []() -> float {
            return Engine::get().getDeltaTime();
            });

        engineNS.set_function("getTotalTime", []() -> float {
            return Engine::get().getTotalTime();
            });

    }
}