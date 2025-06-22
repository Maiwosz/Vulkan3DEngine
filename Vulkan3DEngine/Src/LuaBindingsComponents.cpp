#include "LuaBindingsComponents.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "MaterialComponent.h"
#include "MeshComponent.h"
#include "ScriptComponent.h"
#include "Registry.h"

namespace LuaBindings {

    // Safe component access function that validates before each access
    template<typename T>
    T* safeGetComponent(Registry& registry, Entity entity) {
        if (!registry.entities().valid(entity)) {
            return nullptr;
        }
        if (!registry.components().hasComponent<T>(entity)) {
            return nullptr;
        }
        return &registry.components().getComponent<T>(entity);
    }

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

            // Safe methods that validate entity/component existence
            "setPosition", [](TransformComponent& self, const glm::vec3& pos) {
                // Validate that the component is still valid
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setPosition(pos);
                }
            },
            "getPosition", [](TransformComponent& self) -> glm::vec3 {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getPosition();
                }
                return glm::vec3(0.0f);
            },
            "setRotation", [](TransformComponent& self, const glm::vec3& rot) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setRotation(rot);
                }
            },
            "getRotation", [](TransformComponent& self) -> glm::vec3 {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getRotation();
                }
                return glm::vec3(0.0f);
            },
            "setScale", [](TransformComponent& self, const glm::vec3& scale) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setScale(scale);
                }
            },
            "getScale", [](TransformComponent& self) -> glm::vec3 {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getScale();
                }
                return glm::vec3(1.0f);
            },
            "getModelMatrix", [](TransformComponent& self) -> glm::mat4 {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getWorldMatrix();
                }
                return glm::mat4(1.0f);
            },
            "getViewMatrix", [](TransformComponent& self) -> glm::mat4 {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getViewMatrix();
                }
                return glm::mat4(1.0f);
            },

            // Add validation method
            "isValid", [](TransformComponent& self) -> bool {
                return self.getRegistry() &&
                    self.getRegistry()->entities().valid(self.entity) &&
                    self.getRegistry()->components().hasComponent<TransformComponent>(self.entity);
            }
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

            "setProjectionType", [](CameraComponent& self, CameraComponent::ProjectionType type) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setProjectionType(type);
                }
            },
            "getProjectionType", [](CameraComponent& self) -> CameraComponent::ProjectionType {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getProjectionType();
                }
                return CameraComponent::ProjectionType::Perspective;
            },
            "setAspectRatio", [](CameraComponent& self, float ratio) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setAspectRatio(ratio);
                }
            },
            "getAspectRatio", [](CameraComponent& self) -> float {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getAspectRatio();
                }
                return 16.0f / 9.0f;
            },
            "setVerticalFOV", [](CameraComponent& self, float fov) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setVerticalFOV(fov);
                }
            },
            "getVerticalFOV", [](CameraComponent& self) -> float {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getVerticalFOV();
                }
                return 45.0f;
            },
            "setOrthographicSize", [](CameraComponent& self, float size) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setOrthographicSize(size);
                }
            },
            "getOrthographicSize", [](CameraComponent& self) -> float {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getOrthographicSize();
                }
                return 5.0f;
            },
            "setClippingPlanes", [](CameraComponent& self, float nearPlane, float farPlane) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setClippingPlanes(nearPlane, farPlane);
                }
            },
            "getNearClip", [](CameraComponent& self) -> float {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getNearClip();
                }
                return 0.1f;
            },
            "getFarClip", [](CameraComponent& self) -> float {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getFarClip();
                }
                return 1000.0f;
            },
            "getProjectionMatrix", [](CameraComponent& self) -> glm::mat4 {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getProjectionMatrix();
                }
                return glm::mat4(1.0f);
            },

            "isValid", [](CameraComponent& self) -> bool {
                return self.getRegistry() &&
                    self.getRegistry()->entities().valid(self.entity) &&
                    self.getRegistry()->components().hasComponent<CameraComponent>(self.entity);
            }
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

            "setType", [](LightComponent& self, LightComponent::Type type) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setType(type);
                }
            },
            "getType", [](LightComponent& self) -> LightComponent::Type {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getType();
                }
                return LightComponent::Type::Directional;
            },
            "setColor", [](LightComponent& self, const glm::vec4& color) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setColor(color);
                }
            },
            "getColor", [](LightComponent& self) -> glm::vec3 {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getColor();
                }
                return glm::vec3(1.0f);
            },
            "setDirection", [](LightComponent& self, const glm::vec3& dir) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setDirection(dir);
                }
            },
            "getDirection", [](LightComponent& self) -> glm::vec3 {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getDirection();
                }
                return glm::vec3(0.0f, -1.0f, 0.0f);
            },
            "setRadius", [](LightComponent& self, float radius) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setRadius(radius);
                }
            },
            "getRadius", [](LightComponent& self) -> float {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getRadius();
                }
                return 10.0f;
            },

            "isValid", [](LightComponent& self) -> bool {
                return self.getRegistry() &&
                    self.getRegistry()->entities().valid(self.entity) &&
                    self.getRegistry()->components().hasComponent<LightComponent>(self.entity);
            }
        );
    }

    void registerMaterialComponent(sol::state& state) {
        state.new_usertype<MaterialComponent>("MaterialComponent",
            sol::constructors<MaterialComponent()>(),

            // Handle AssetHandle properly - provide overloads for both string and AssetHandle
            "setMaterial", sol::overload(
                // Accept string and convert to AssetHandle
                [](MaterialComponent& self, const std::string& material) {
                    if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                        AssetHandle handle(AssetType::Material, material);
                        self.setMaterial(handle);
                    }
                },
                // Accept AssetHandle directly
                [](MaterialComponent& self, const AssetHandle& handle) {
                    if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                        self.setMaterial(handle);
                    }
                }
            ),

            "getMaterial", [](MaterialComponent& self) -> AssetHandle {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getMaterial();
                }
                return AssetHandle(); // Return default AssetHandle
            },

            // Convenience method to get material filename as string
            "getMaterialName", [](MaterialComponent& self) -> std::string {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getMaterial().filename;
                }
                return "";
            },

            "isValid", [](MaterialComponent& self) -> bool {
                return self.getRegistry() &&
                    self.getRegistry()->entities().valid(self.entity) &&
                    self.getRegistry()->components().hasComponent<MaterialComponent>(self.entity);
            }
        );
    }

    void registerMeshComponent(sol::state& state) {
        state.new_usertype<MeshComponent>("MeshComponent",
            sol::constructors<MeshComponent()>(),

            // Handle AssetHandle properly - provide overloads for both string and AssetHandle
            "setMesh", sol::overload(
                // Accept string and convert to AssetHandle
                [](MeshComponent& self, const std::string& mesh) {
                    if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                        AssetHandle handle(AssetType::Mesh, mesh);
                        self.setMesh(handle);
                    }
                },
                // Accept AssetHandle directly
                [](MeshComponent& self, const AssetHandle& handle) {
                    if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                        self.setMesh(handle);
                    }
                }
            ),

            "getMesh", [](MeshComponent& self) -> AssetHandle {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getMesh();
                }
                return AssetHandle(); // Return default AssetHandle
            },

            // Convenience method to get mesh filename as string
            "getMeshName", [](MeshComponent& self) -> std::string {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getMesh().filename;
                }
                return "";
            },

            "isValid", [](MeshComponent& self) -> bool {
                return self.getRegistry() &&
                    self.getRegistry()->entities().valid(self.entity) &&
                    self.getRegistry()->components().hasComponent<MeshComponent>(self.entity);
            }
        );
    }

    void registerScriptComponent(sol::state& state) {
        state.new_usertype<ScriptComponent>("ScriptComponent",
            sol::constructors<ScriptComponent()>(),

            "setScriptPath", [](ScriptComponent& self, const std::string& path) {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    self.setScript(path);
                }
            },
            "getScriptPath", [](ScriptComponent& self) -> std::string {
                if (self.getRegistry() && self.getRegistry()->entities().valid(self.entity)) {
                    return self.getScript();
                }
                return "";
            },

            "isValid", [](ScriptComponent& self) -> bool {
                return self.getRegistry() &&
                    self.getRegistry()->entities().valid(self.entity) &&
                    self.getRegistry()->components().hasComponent<ScriptComponent>(self.entity);
            }
        );
    }
}