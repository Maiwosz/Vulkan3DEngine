#include "LuaBindings.h"

namespace LuaBindings {
    void registerAll(sol::state& state, Engine& engine, Registry& registry) {
        registerCoreTypes(state);
        registerEngineFunctions(state, engine, registry);
        registerSpdlogFunctions(state);
        registerInputSystem(state, engine);
        registerRegistry(state);
        registerComponents(state);
    }
}