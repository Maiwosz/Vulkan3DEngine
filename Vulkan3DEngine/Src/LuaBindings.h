#pragma once
#include <sol/sol.hpp>

// Forward declarations
class Registry;

namespace LuaBindings {
    // Initialize all bindings
    void registerAll(sol::state& state);
}

// These are the new header files we'll include from the main binding file
#include "LuaBindingsCoreTypes.h"
#include "LuaBindingsEngine.h"
#include "LuaBindingsInput.h"
#include "LuaBindingsComponents.h"
#include "LuaBindingsSpdlog.h"
#include "LuaBindingsRegistry.h"
