#pragma once
#include <sol/sol.hpp>
#include "Engine.h"
#include "InputSystem.h"

namespace LuaBindings {
    // Register input system functionality
    void registerInputSystem(sol::state& state, Engine& engine);
}