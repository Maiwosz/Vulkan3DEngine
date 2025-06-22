#include "LuaBindingsRegistry.h"
#include "Registry.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "MaterialComponent.h"
#include "MeshComponent.h"
#include "ScriptComponent.h"

namespace LuaBindings {

    // Helper class to wrap component access with lazy binding
    template<typename T>
    class ComponentWrapper {
    public:
        ComponentWrapper(Registry& registry, Entity entity)
            : m_registry(registry), m_entity(entity) {
        }

        T* get() {
            if (!m_registry.components().hasComponent<T>(m_entity)) {
                return nullptr;
            }
            return &m_registry.components().getComponent<T>(m_entity);
        }

        bool valid() const {
            return m_registry.entities().valid(m_entity) &&
                m_registry.components().hasComponent<T>(m_entity);
        }

        Entity getEntity() const { return m_entity; }
        Registry& getRegistry() const { return m_registry; }

    private:
        Registry& m_registry;
        Entity m_entity;
    };

    void registerRegistry(sol::state& state) {
        // Register the ComponentWrapper types with forwarded methods
        registerTransformWrapper(state);
        registerCameraWrapper(state);
        registerLightWrapper(state);
        registerMaterialWrapper(state);
        registerMeshWrapper(state);
        registerScriptWrapper(state);

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

        // Component existence checks - these are safe as they don't return pointers
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

        // Component addition - returns wrappers instead of direct pointers
        registryType.set_function("addComponent", sol::overload(
            [&state](Registry& reg, Entity e, const std::string& type) -> sol::object {
                if (type == "Transform") {
                    reg.components().addComponent<TransformComponent>(e);
                    return sol::make_object(state, ComponentWrapper<TransformComponent>(reg, e));
                }
                if (type == "Camera") {
                    reg.components().addComponent<CameraComponent>(e);
                    return sol::make_object(state, ComponentWrapper<CameraComponent>(reg, e));
                }
                if (type == "Light") {
                    reg.components().addComponent<LightComponent>(e);
                    return sol::make_object(state, ComponentWrapper<LightComponent>(reg, e));
                }
                if (type == "Material") {
                    reg.components().addComponent<MaterialComponent>(e);
                    return sol::make_object(state, ComponentWrapper<MaterialComponent>(reg, e));
                }
                if (type == "Mesh") {
                    reg.components().addComponent<MeshComponent>(e);
                    return sol::make_object(state, ComponentWrapper<MeshComponent>(reg, e));
                }
                return sol::nil;
            },
            [&state](Registry& reg, Entity e, const std::string& type, const std::string& path) -> sol::object {
                if (type == "Script") {
                    auto& comp = reg.components().addComponent<ScriptComponent>(e);
                    comp.setScript(path);
                    return sol::make_object(state, ComponentWrapper<ScriptComponent>(reg, e));
                }
                return sol::nil;
            }
        ));

        // Component access - returns wrappers instead of direct pointers
        registryType.set_function("getComponent", sol::overload(
            [&state](Registry& reg, Entity e, const std::string& type) -> sol::object {
                if (type == "Transform" && reg.components().hasComponent<TransformComponent>(e))
                    return sol::make_object(state, ComponentWrapper<TransformComponent>(reg, e));
                if (type == "Camera" && reg.components().hasComponent<CameraComponent>(e))
                    return sol::make_object(state, ComponentWrapper<CameraComponent>(reg, e));
                if (type == "Light" && reg.components().hasComponent<LightComponent>(e))
                    return sol::make_object(state, ComponentWrapper<LightComponent>(reg, e));
                if (type == "Material" && reg.components().hasComponent<MaterialComponent>(e))
                    return sol::make_object(state, ComponentWrapper<MaterialComponent>(reg, e));
                if (type == "Mesh" && reg.components().hasComponent<MeshComponent>(e))
                    return sol::make_object(state, ComponentWrapper<MeshComponent>(reg, e));
                if (type == "Script" && reg.components().hasComponent<ScriptComponent>(e))
                    return sol::make_object(state, ComponentWrapper<ScriptComponent>(reg, e));

                return sol::nil;
            }
        ));

        // Component removal
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

    void registerTransformWrapper(sol::state& state) {
        state.new_usertype<ComponentWrapper<TransformComponent>>("TransformWrapper",
            sol::constructors<ComponentWrapper<TransformComponent>(Registry&, Entity)>(),
            "valid", &ComponentWrapper<TransformComponent>::valid,
            "get", [](ComponentWrapper<TransformComponent>& wrapper) -> TransformComponent* {
                return wrapper.get();
            },
            "entity", [](ComponentWrapper<TransformComponent>& wrapper) {
                return wrapper.getEntity();
            },

            // Forward TransformComponent methods
            "setPosition", [](ComponentWrapper<TransformComponent>& wrapper, const glm::vec3& pos) {
                if (auto* comp = wrapper.get()) {
                    comp->setPosition(pos);
                }
            },
            "getPosition", [](ComponentWrapper<TransformComponent>& wrapper) -> glm::vec3 {
                if (auto* comp = wrapper.get()) {
                    return comp->getPosition();
                }
                return glm::vec3(0.0f);
            },
            "setRotation", [](ComponentWrapper<TransformComponent>& wrapper, const glm::vec3& rot) {
                if (auto* comp = wrapper.get()) {
                    comp->setRotation(rot);
                }
            },
            "getRotation", [](ComponentWrapper<TransformComponent>& wrapper) -> glm::vec3 {
                if (auto* comp = wrapper.get()) {
                    return comp->getRotation();
                }
                return glm::vec3(0.0f);
            },
            "setScale", [](ComponentWrapper<TransformComponent>& wrapper, const glm::vec3& scale) {
                if (auto* comp = wrapper.get()) {
                    comp->setScale(scale);
                }
            },
            "getScale", [](ComponentWrapper<TransformComponent>& wrapper) -> glm::vec3 {
                if (auto* comp = wrapper.get()) {
                    return comp->getScale();
                }
                return glm::vec3(1.0f);
            },
            "getModelMatrix", [](ComponentWrapper<TransformComponent>& wrapper) -> glm::mat4 {
                if (auto* comp = wrapper.get()) {
                    return comp->getWorldMatrix();
                }
                return glm::mat4(1.0f);
            },
            "getViewMatrix", [](ComponentWrapper<TransformComponent>& wrapper) -> glm::mat4 {
                if (auto* comp = wrapper.get()) {
                    return comp->getViewMatrix();
                }
                return glm::mat4(1.0f);
            },
            "isValid", [](ComponentWrapper<TransformComponent>& wrapper) -> bool {
                return wrapper.valid();
            }
        );
    }

    void registerCameraWrapper(sol::state& state) {
        state.new_usertype<ComponentWrapper<CameraComponent>>("CameraWrapper",
            sol::constructors<ComponentWrapper<CameraComponent>(Registry&, Entity)>(),
            "valid", &ComponentWrapper<CameraComponent>::valid,
            "get", [](ComponentWrapper<CameraComponent>& wrapper) -> CameraComponent* {
                return wrapper.get();
            },
            "entity", [](ComponentWrapper<CameraComponent>& wrapper) {
                return wrapper.getEntity();
            },

            // Forward CameraComponent methods
            "setProjectionType", [](ComponentWrapper<CameraComponent>& wrapper, CameraComponent::ProjectionType type) {
                if (auto* comp = wrapper.get()) {
                    comp->setProjectionType(type);
                }
            },
            "getProjectionType", [](ComponentWrapper<CameraComponent>& wrapper) -> CameraComponent::ProjectionType {
                if (auto* comp = wrapper.get()) {
                    return comp->getProjectionType();
                }
                return CameraComponent::ProjectionType::Perspective;
            },
            "setAspectRatio", [](ComponentWrapper<CameraComponent>& wrapper, float ratio) {
                if (auto* comp = wrapper.get()) {
                    comp->setAspectRatio(ratio);
                }
            },
            "getAspectRatio", [](ComponentWrapper<CameraComponent>& wrapper) -> float {
                if (auto* comp = wrapper.get()) {
                    return comp->getAspectRatio();
                }
                return 16.0f / 9.0f;
            },
            "setVerticalFOV", [](ComponentWrapper<CameraComponent>& wrapper, float fov) {
                if (auto* comp = wrapper.get()) {
                    comp->setVerticalFOV(fov);
                }
            },
            "getVerticalFOV", [](ComponentWrapper<CameraComponent>& wrapper) -> float {
                if (auto* comp = wrapper.get()) {
                    return comp->getVerticalFOV();
                }
                return 45.0f;
            },
            "setOrthographicSize", [](ComponentWrapper<CameraComponent>& wrapper, float size) {
                if (auto* comp = wrapper.get()) {
                    comp->setOrthographicSize(size);
                }
            },
            "getOrthographicSize", [](ComponentWrapper<CameraComponent>& wrapper) -> float {
                if (auto* comp = wrapper.get()) {
                    return comp->getOrthographicSize();
                }
                return 5.0f;
            },
            "setClippingPlanes", [](ComponentWrapper<CameraComponent>& wrapper, float nearPlane, float farPlane) {
                if (auto* comp = wrapper.get()) {
                    comp->setClippingPlanes(nearPlane, farPlane);
                }
            },
            "getNearClip", [](ComponentWrapper<CameraComponent>& wrapper) -> float {
                if (auto* comp = wrapper.get()) {
                    return comp->getNearClip();
                }
                return 0.1f;
            },
            "getFarClip", [](ComponentWrapper<CameraComponent>& wrapper) -> float {
                if (auto* comp = wrapper.get()) {
                    return comp->getFarClip();
                }
                return 1000.0f;
            },
            "getProjectionMatrix", [](ComponentWrapper<CameraComponent>& wrapper) -> glm::mat4 {
                if (auto* comp = wrapper.get()) {
                    return comp->getProjectionMatrix();
                }
                return glm::mat4(1.0f);
            },
            "isValid", [](ComponentWrapper<CameraComponent>& wrapper) -> bool {
                return wrapper.valid();
            }
        );
    }

    void registerLightWrapper(sol::state& state) {
        state.new_usertype<ComponentWrapper<LightComponent>>("LightWrapper",
            sol::constructors<ComponentWrapper<LightComponent>(Registry&, Entity)>(),
            "valid", &ComponentWrapper<LightComponent>::valid,
            "get", [](ComponentWrapper<LightComponent>& wrapper) -> LightComponent* {
                return wrapper.get();
            },
            "entity", [](ComponentWrapper<LightComponent>& wrapper) {
                return wrapper.getEntity();
            },

            // Forward LightComponent methods
            "setType", [](ComponentWrapper<LightComponent>& wrapper, LightComponent::Type type) {
                if (auto* comp = wrapper.get()) {
                    comp->setType(type);
                }
            },
            "getType", [](ComponentWrapper<LightComponent>& wrapper) -> LightComponent::Type {
                if (auto* comp = wrapper.get()) {
                    return comp->getType();
                }
                return LightComponent::Type::Directional;
            },
            "setColor", [](ComponentWrapper<LightComponent>& wrapper, const glm::vec4& color) {
                if (auto* comp = wrapper.get()) {
                    comp->setColor(color);
                }
            },
            "getColor", [](ComponentWrapper<LightComponent>& wrapper) -> glm::vec3 {
                if (auto* comp = wrapper.get()) {
                    return comp->getColor();
                }
                return glm::vec3(1.0f);
            },
            "setDirection", [](ComponentWrapper<LightComponent>& wrapper, const glm::vec3& dir) {
                if (auto* comp = wrapper.get()) {
                    comp->setDirection(dir);
                }
            },
            "getDirection", [](ComponentWrapper<LightComponent>& wrapper) -> glm::vec3 {
                if (auto* comp = wrapper.get()) {
                    return comp->getDirection();
                }
                return glm::vec3(0.0f, -1.0f, 0.0f);
            },
            "setRadius", [](ComponentWrapper<LightComponent>& wrapper, float radius) {
                if (auto* comp = wrapper.get()) {
                    comp->setRadius(radius);
                }
            },
            "getRadius", [](ComponentWrapper<LightComponent>& wrapper) -> float {
                if (auto* comp = wrapper.get()) {
                    return comp->getRadius();
                }
                return 10.0f;
            },
            "isValid", [](ComponentWrapper<LightComponent>& wrapper) -> bool {
                return wrapper.valid();
            }
        );
    }

    void registerMaterialWrapper(sol::state& state) {
        state.new_usertype<ComponentWrapper<MaterialComponent>>("MaterialWrapper",
            sol::constructors<ComponentWrapper<MaterialComponent>(Registry&, Entity)>(),
            "valid", &ComponentWrapper<MaterialComponent>::valid,
            "get", [](ComponentWrapper<MaterialComponent>& wrapper) -> MaterialComponent* {
                return wrapper.get();
            },
            "entity", [](ComponentWrapper<MaterialComponent>& wrapper) {
                return wrapper.getEntity();
            },

            // Forward MaterialComponent methods
            "setMaterial", sol::overload(
                [](ComponentWrapper<MaterialComponent>& wrapper, const std::string& material) {
                    if (auto* comp = wrapper.get()) {
                        AssetHandle handle(AssetType::Material, material);
                        comp->setMaterial(handle);
                    }
                },
                [](ComponentWrapper<MaterialComponent>& wrapper, const AssetHandle& handle) {
                    if (auto* comp = wrapper.get()) {
                        comp->setMaterial(handle);
                    }
                }
            ),
            "getMaterial", [](ComponentWrapper<MaterialComponent>& wrapper) -> AssetHandle {
                if (auto* comp = wrapper.get()) {
                    return comp->getMaterial();
                }
                return AssetHandle();
            },
            "getMaterialName", [](ComponentWrapper<MaterialComponent>& wrapper) -> std::string {
                if (auto* comp = wrapper.get()) {
                    return comp->getMaterial().filename;
                }
                return "";
            },
            "isValid", [](ComponentWrapper<MaterialComponent>& wrapper) -> bool {
                return wrapper.valid();
            }
        );
    }

    void registerMeshWrapper(sol::state& state) {
        state.new_usertype<ComponentWrapper<MeshComponent>>("MeshWrapper",
            sol::constructors<ComponentWrapper<MeshComponent>(Registry&, Entity)>(),
            "valid", &ComponentWrapper<MeshComponent>::valid,
            "get", [](ComponentWrapper<MeshComponent>& wrapper) -> MeshComponent* {
                return wrapper.get();
            },
            "entity", [](ComponentWrapper<MeshComponent>& wrapper) {
                return wrapper.getEntity();
            },

            // Forward MeshComponent methods
            "setMesh", sol::overload(
                [](ComponentWrapper<MeshComponent>& wrapper, const std::string& mesh) {
                    if (auto* comp = wrapper.get()) {
                        AssetHandle handle(AssetType::Mesh, mesh);
                        comp->setMesh(handle);
                    }
                },
                [](ComponentWrapper<MeshComponent>& wrapper, const AssetHandle& handle) {
                    if (auto* comp = wrapper.get()) {
                        comp->setMesh(handle);
                    }
                }
            ),
            "getMesh", [](ComponentWrapper<MeshComponent>& wrapper) -> AssetHandle {
                if (auto* comp = wrapper.get()) {
                    return comp->getMesh();
                }
                return AssetHandle();
            },
            "getMeshName", [](ComponentWrapper<MeshComponent>& wrapper) -> std::string {
                if (auto* comp = wrapper.get()) {
                    return comp->getMesh().filename;
                }
                return "";
            },
            "isValid", [](ComponentWrapper<MeshComponent>& wrapper) -> bool {
                return wrapper.valid();
            }
        );
    }

    void registerScriptWrapper(sol::state& state) {
        state.new_usertype<ComponentWrapper<ScriptComponent>>("ScriptWrapper",
            sol::constructors<ComponentWrapper<ScriptComponent>(Registry&, Entity)>(),
            "valid", &ComponentWrapper<ScriptComponent>::valid,
            "get", [](ComponentWrapper<ScriptComponent>& wrapper) -> ScriptComponent* {
                return wrapper.get();
            },
            "entity", [](ComponentWrapper<ScriptComponent>& wrapper) {
                return wrapper.getEntity();
            },

            // Forward ScriptComponent methods
            "setScriptPath", [](ComponentWrapper<ScriptComponent>& wrapper, const std::string& path) {
                if (auto* comp = wrapper.get()) {
                    comp->setScript(path);
                }
            },
            "getScriptPath", [](ComponentWrapper<ScriptComponent>& wrapper) -> std::string {
                if (auto* comp = wrapper.get()) {
                    return comp->getScript();
                }
                return "";
            },
            "isValid", [](ComponentWrapper<ScriptComponent>& wrapper) -> bool {
                return wrapper.valid();
            }
        );
    }
}