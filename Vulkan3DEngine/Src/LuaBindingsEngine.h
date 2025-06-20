#pragma once
#include <sol/sol.hpp>

class Engine;
class Registry;

namespace LuaBindings {
    // Register engine functionality
    void registerEngineFunctions(sol::state& state, Engine& engine, Registry& registry);
}