#pragma once

#include <sol/sol.hpp>
#include "Entity.h"

class Registry;

namespace LuaBindings {

    void registerRegistry(sol::state& state);

    // Wrapper registration functions
    void registerTransformWrapper(sol::state& state);
    void registerCameraWrapper(sol::state& state);
    void registerLightWrapper(sol::state& state);
    void registerMaterialWrapper(sol::state& state);
    void registerMeshWrapper(sol::state& state);
    void registerScriptWrapper(sol::state& state);
}