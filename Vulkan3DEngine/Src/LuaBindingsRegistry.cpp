#include "LuaBindingsRegistry.h"
#include "Registry.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "MaterialComponent.h"
#include "MeshComponent.h"
#include "ScriptComponent.h"

namespace LuaBindings {
    void registerRegistry(sol::state& state) {
        auto registryType = state.new_usertype<Registry>("Registry",
            sol::no_constructor,
            "create", sol::overload(
                static_cast<Entity(Registry::*)(const std::string&)>(&Registry::create)
            ),
            "destroy", &Registry::destroy,
            "valid", &Registry::valid
        );

        // Generic functions for component management
        registryType.set_function("hasComponent", sol::overload(
            [](Registry& reg, Entity e, const std::string& type) {
                if (type == "Transform") return reg.hasComponent<TransformComponent>(e);
                if (type == "Camera") return reg.hasComponent<CameraComponent>(e);
                if (type == "Light") return reg.hasComponent<LightComponent>(e);
                if (type == "Material") return reg.hasComponent<MaterialComponent>(e);
                if (type == "Mesh") return reg.hasComponent<MeshComponent>(e);
                if (type == "Script") return reg.hasComponent<ScriptComponent>(e);
                return false;
            }
        ));

        registryType.set_function("addComponent", sol::overload(
            [&state](Registry& reg, Entity e, const std::string& type) -> sol::object {
                if (type == "Transform")
                    return sol::make_object(state, &reg.addComponent<TransformComponent>(e));
                if (type == "Camera")
                    return sol::make_object(state, &reg.addComponent<CameraComponent>(e));
                if (type == "Light")
                    return sol::make_object(state, &reg.addComponent<LightComponent>(e));
                if (type == "Material")
                    return sol::make_object(state, &reg.addComponent<MaterialComponent>(e));
                if (type == "Mesh")
                    return sol::make_object(state, &reg.addComponent<MeshComponent>(e));

                return sol::nil;
            },
            [&state](Registry& reg, Entity e, const std::string& type, const std::string& path) -> sol::object {
                if (type == "Script") {
                    auto& comp = reg.addComponent<ScriptComponent>(e);
                    comp.setScript(path);
                    return sol::make_object(state, &comp);
                }

                return sol::nil;
            }
        ));

        registryType.set_function("getComponent", sol::overload(
            [&state](Registry& reg, Entity e, const std::string& type) -> sol::object {
                if (type == "Transform")
                    return sol::make_object(state, &reg.getComponent<TransformComponent>(e));
                if (type == "Camera")
                    return sol::make_object(state, &reg.getComponent<CameraComponent>(e));
                if (type == "Light")
                    return sol::make_object(state, &reg.getComponent<LightComponent>(e));
                if (type == "Material")
                    return sol::make_object(state, &reg.getComponent<MaterialComponent>(e));
                if (type == "Mesh")
                    return sol::make_object(state, &reg.getComponent<MeshComponent>(e));
                if (type == "Script")
                    return sol::make_object(state, &reg.getComponent<ScriptComponent>(e));

                return sol::nil;
            }
        ));

        registryType.set_function("removeComponent", sol::overload(
            [](Registry& reg, Entity e, const std::string& type) {
                if (type == "Transform") reg.removeComponent<TransformComponent>(e);
                else if (type == "Camera") reg.removeComponent<CameraComponent>(e);
                else if (type == "Light") reg.removeComponent<LightComponent>(e);
                else if (type == "Material") reg.removeComponent<MaterialComponent>(e);
                else if (type == "Mesh") reg.removeComponent<MeshComponent>(e);
                else if (type == "Script") reg.removeComponent<ScriptComponent>(e);
            }
        ));
    }
}