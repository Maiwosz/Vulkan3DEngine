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
                [](Registry& reg, const std::string& name) {
                    return reg.entities().create(name);
                }
            ),
            "destroy", [](Registry& reg, Entity entity) {
                reg.entities().destroy(entity);
            },
            "valid", [](Registry& reg, Entity entity) {
                return reg.entities().valid(entity);
            }
        );

        // Generic functions for component management
        registryType.set_function("hasComponent", sol::overload(
            [](Registry& reg, Entity e, const std::string& type) {
                if (type == "Transform") return reg.components().hasComponent<TransformComponent>(e);
                if (type == "Camera") return reg.components().hasComponent<CameraComponent>(e);
                if (type == "Light") return reg.components().hasComponent<LightComponent>(e);
                if (type == "Material") return reg.components().hasComponent<MaterialComponent>(e);
                if (type == "Mesh") return reg.components().hasComponent<MeshComponent>(e);
                if (type == "Script") return reg.components().hasComponent<ScriptComponent>(e);
                return false;
            }
        ));

        registryType.set_function("addComponent", sol::overload(
            [&state](Registry& reg, Entity e, const std::string& type) -> sol::object {
                if (type == "Transform")
                    return sol::make_object(state, &reg.components().addComponent<TransformComponent>(e));
                if (type == "Camera")
                    return sol::make_object(state, &reg.components().addComponent<CameraComponent>(e));
                if (type == "Light")
                    return sol::make_object(state, &reg.components().addComponent<LightComponent>(e));
                if (type == "Material")
                    return sol::make_object(state, &reg.components().addComponent<MaterialComponent>(e));
                if (type == "Mesh")
                    return sol::make_object(state, &reg.components().addComponent<MeshComponent>(e));

                return sol::nil;
            },
            [&state](Registry& reg, Entity e, const std::string& type, const std::string& path) -> sol::object {
                if (type == "Script") {
                    auto& comp = reg.components().addComponent<ScriptComponent>(e);
                    comp.setScript(path);
                    return sol::make_object(state, &comp);
                }

                return sol::nil;
            }
        ));

        registryType.set_function("getComponent", sol::overload(
            [&state](Registry& reg, Entity e, const std::string& type) -> sol::object {
                if (type == "Transform")
                    return sol::make_object(state, &reg.components().getComponent<TransformComponent>(e));
                if (type == "Camera")
                    return sol::make_object(state, &reg.components().getComponent<CameraComponent>(e));
                if (type == "Light")
                    return sol::make_object(state, &reg.components().getComponent<LightComponent>(e));
                if (type == "Material")
                    return sol::make_object(state, &reg.components().getComponent<MaterialComponent>(e));
                if (type == "Mesh")
                    return sol::make_object(state, &reg.components().getComponent<MeshComponent>(e));
                if (type == "Script")
                    return sol::make_object(state, &reg.components().getComponent<ScriptComponent>(e));

                return sol::nil;
            }
        ));

        registryType.set_function("removeComponent", sol::overload(
            [](Registry& reg, Entity e, const std::string& type) {
                if (type == "Transform") reg.components().removeComponent<TransformComponent>(e);
                else if (type == "Camera") reg.components().removeComponent<CameraComponent>(e);
                else if (type == "Light") reg.components().removeComponent<LightComponent>(e);
                else if (type == "Material") reg.components().removeComponent<MaterialComponent>(e);
                else if (type == "Mesh") reg.components().removeComponent<MeshComponent>(e);
                else if (type == "Script") reg.components().removeComponent<ScriptComponent>(e);
            }
        ));
    }
}