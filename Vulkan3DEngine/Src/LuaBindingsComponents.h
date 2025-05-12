#pragma once
#include <sol/sol.hpp>

namespace LuaBindings {
    // Register all components
    void registerComponents(sol::state& state);

    // Register individual component types
    void registerTransformComponent(sol::state& state);
    void registerCameraComponent(sol::state& state);
    void registerLightComponent(sol::state& state);
    void registerMaterialComponent(sol::state& state);
    void registerMeshComponent(sol::state& state);
    void registerScriptComponent(sol::state& state);
}