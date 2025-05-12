#pragma once

#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

namespace LuaBindings {
    void registerSpdlogFunctions(sol::state& state);
}