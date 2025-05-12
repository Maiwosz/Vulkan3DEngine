#include "LuaBindingsComponents.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "MaterialComponent.h"
#include "MeshComponent.h"
#include "ScriptComponent.h"

namespace LuaBindings {
    void registerComponents(sol::state& state) {
        registerTransformComponent(state);
        registerCameraComponent(state);
        registerLightComponent(state);
        registerMaterialComponent(state);
        registerMeshComponent(state);
        registerScriptComponent(state);
    }

    void registerTransformComponent(sol::state& state) {
        state.new_usertype<TransformComponent>("TransformComponent",
            sol::constructors<TransformComponent()>(),
            "setPosition", [](TransformComponent& self, const glm::vec3& pos) { self.setPosition(pos); },
            "getPosition", &TransformComponent::getPosition,
            "setRotation", [](TransformComponent& self, const glm::vec3& rot) { self.setRotation(rot); },
            "getRotation", &TransformComponent::getRotation,
            "setScale", [](TransformComponent& self, const glm::vec3& scale) { self.setScale(scale); },
            "getScale", &TransformComponent::getScale,
            "getModelMatrix", &TransformComponent::getModelMatrix,
            "getViewMatrix", &TransformComponent::getViewMatrix
        );
    }

    void registerCameraComponent(sol::state& state) {
        state.new_enum<CameraComponent::ProjectionType>("ProjectionType",
            {
                {"Perspective", CameraComponent::ProjectionType::Perspective},
                {"Orthographic", CameraComponent::ProjectionType::Orthographic}
            }
        );

        state.new_usertype<CameraComponent>("CameraComponent",
            sol::constructors<CameraComponent()>(),
            "setProjectionType", &CameraComponent::setProjectionType,
            "getProjectionType", &CameraComponent::getProjectionType,
            "setAspectRatio", &CameraComponent::setAspectRatio,
            "getAspectRatio", &CameraComponent::getAspectRatio,
            "setVerticalFOV", &CameraComponent::setVerticalFOV,
            "getVerticalFOV", &CameraComponent::getVerticalFOV,
            "setOrthographicSize", &CameraComponent::setOrthographicSize,
            "getOrthographicSize", &CameraComponent::getOrthographicSize,
            "setClippingPlanes", &CameraComponent::setClippingPlanes,
            "getNearClip", &CameraComponent::getNearClip,
            "getFarClip", &CameraComponent::getFarClip,
            "getProjectionMatrix", &CameraComponent::getProjectionMatrix
        );
    }

    void registerLightComponent(sol::state& state) {
        state.new_enum<LightComponent::Type>("LightType",
            {
                {"Directional", LightComponent::Type::Directional},
                {"Point", LightComponent::Type::Point}
            }
        );

        state.new_usertype<LightComponent>("LightComponent",
            sol::constructors<LightComponent(), LightComponent(LightComponent::Type)>(),
            "setType", &LightComponent::setType,
            "getType", &LightComponent::getType,
            "setColor", &LightComponent::setColor,
            "getColor", &LightComponent::getColor,
            "setDirection", &LightComponent::setDirection,
            "getDirection", &LightComponent::getDirection,
            "setRadius", &LightComponent::setRadius,
            "getRadius", &LightComponent::getRadius
        );
    }

    void registerMaterialComponent(sol::state& state) {
        state.new_usertype<MaterialComponent>("MaterialComponent",
            sol::constructors<MaterialComponent()>(),
            "setMaterial", &MaterialComponent::setMaterial,
            "getMaterial", &MaterialComponent::getMaterial
        );
    }

    void registerMeshComponent(sol::state& state) {
        state.new_usertype<MeshComponent>("MeshComponent",
            sol::constructors<MeshComponent()>(),
            "setMesh", &MeshComponent::setMesh,
            "getMesh", &MeshComponent::getMesh
        );
    }

    void registerScriptComponent(sol::state& state) {
        state.new_usertype<ScriptComponent>("ScriptComponent",
            sol::constructors<ScriptComponent()>(),
            "setScriptPath", &ScriptComponent::setScriptPath,
            "getScriptPath", &ScriptComponent::getScriptPath
        );
    }
}