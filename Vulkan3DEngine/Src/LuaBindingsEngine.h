#pragma once
#include <sol/sol.hpp>

namespace LuaBindings {
    // Register engine functionality
    void registerEngineFunctions(sol::state& state);
}