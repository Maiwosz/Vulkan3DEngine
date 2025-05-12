#include "LuaBindings.h"

namespace LuaBindings {
    void registerAll(sol::state& state) {
        registerCoreTypes(state);
        registerEngineFunctions(state);
        registerSpdlogFunctions(state);
        registerInputSystem(state);
        registerRegistry(state);
        registerComponents(state);
    }
}