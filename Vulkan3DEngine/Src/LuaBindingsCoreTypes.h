#pragma once
#include <sol/sol.hpp>

namespace LuaBindings {
    // Register core types like Entity, Vector3, etc.
    void registerCoreTypes(sol::state& state);
}