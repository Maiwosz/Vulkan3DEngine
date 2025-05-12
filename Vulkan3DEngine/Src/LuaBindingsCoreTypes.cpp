#include "LuaBindingsCoreTypes.h"
#include "Entity.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "AssetHandle.h"

namespace LuaBindings {
    void registerCoreTypes(sol::state& state) {
        // Register Entity type
        state.new_usertype<Entity>("Entity",
            "id", &Entity::id,
            sol::meta_function::equal_to, [](const Entity& a, const Entity& b) { return a == b; }
        );

        // Register AssetType enum
        state.new_enum<AssetType>("AssetType",
            {
                {"Texture", AssetType::Texture},
                {"Mesh", AssetType::Mesh},
                {"Material", AssetType::Material},
                {"Shader", AssetType::Shader}
            }
        );

        // Register AssetHandle type
        state.new_usertype<AssetHandle>("AssetHandle",
            sol::constructors<AssetHandle(), AssetHandle(AssetType, const std::string&)>(),
            "type", &AssetHandle::type,
            "filename", &AssetHandle::filename,
            sol::meta_function::equal_to, [](const AssetHandle& a, const AssetHandle& b) { return a == b; }
        );

        // Make Vector3 available as a global constructor function
        state["Vector3"] = sol::overload(
            []() -> glm::vec3 { return glm::vec3(0.0f); },
            [](float x) -> glm::vec3 { return glm::vec3(x); },
            [](float x, float y, float z) -> glm::vec3 { return glm::vec3(x, y, z); }
        );

        // Make Vector4 available as a global constructor function
        state["Vector4"] = sol::overload(
            []() -> glm::vec4 { return glm::vec4(0.0f); },
            [](float x) -> glm::vec4 { return glm::vec4(x); },
            [](float x, float y, float z, float w) -> glm::vec4 { return glm::vec4(x, y, z, w); },
            [](const glm::vec3& v, float w) -> glm::vec4 { return glm::vec4(v, w); }
        );

        // Make Vector2 available as a global constructor function
        state["Vector2"] = sol::overload(
            []() -> glm::vec2 { return glm::vec2(0.0f); },
            [](float x) -> glm::vec2 { return glm::vec2(x); },
            [](float x, float y) -> glm::vec2 { return glm::vec2(x, y); }
        );

        // Register the actual Vector3 type
        state.new_usertype<glm::vec3>("vec3",
            sol::constructors<glm::vec3(), glm::vec3(float), glm::vec3(float, float, float)>(),
            "x", &glm::vec3::x,
            "y", &glm::vec3::y,
            "z", &glm::vec3::z,
            sol::meta_function::addition, [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
            sol::meta_function::subtraction, [](const glm::vec3& a, const glm::vec3& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const glm::vec3& a, const glm::vec3& b) { return a * b; },
                [](const glm::vec3& a, float b) { return a * b; },
                [](float a, const glm::vec3& b) { return a * b; }
            ),
            sol::meta_function::division, sol::overload(
                [](const glm::vec3& a, const glm::vec3& b) { return a / b; },
                [](const glm::vec3& a, float b) { return a / b; }
            ),
            "normalize", [](const glm::vec3& v) { return glm::normalize(v); },
            "length", [](const glm::vec3& v) { return glm::length(v); },
            "dot", [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); },
            "cross", [](const glm::vec3& a, const glm::vec3& b) { return glm::cross(a, b); }
        );

        // Register the actual Vector4 type
        state.new_usertype<glm::vec4>("vec4",
            sol::constructors<glm::vec4(), glm::vec4(float), glm::vec4(float, float, float, float), glm::vec4(const glm::vec3&, float)>(),
            "x", &glm::vec4::x,
            "y", &glm::vec4::y,
            "z", &glm::vec4::z,
            "w", &glm::vec4::w,
            sol::meta_function::addition, [](const glm::vec4& a, const glm::vec4& b) { return a + b; },
            sol::meta_function::subtraction, [](const glm::vec4& a, const glm::vec4& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const glm::vec4& a, const glm::vec4& b) { return a * b; },
                [](const glm::vec4& a, float b) { return a * b; },
                [](float a, const glm::vec4& b) { return a * b; }
            ),
            sol::meta_function::division, sol::overload(
                [](const glm::vec4& a, const glm::vec4& b) { return a / b; },
                [](const glm::vec4& a, float b) { return a / b; }
            ),
            "normalize", [](const glm::vec4& v) { return glm::normalize(v); },
            "length", [](const glm::vec4& v) { return glm::length(v); }
        );

        // Register the actual Vector2 type
        state.new_usertype<glm::vec2>("vec2",
            sol::constructors<glm::vec2(), glm::vec2(float), glm::vec2(float, float)>(),
            "x", &glm::vec2::x,
            "y", &glm::vec2::y,
            sol::meta_function::addition, [](const glm::vec2& a, const glm::vec2& b) { return a + b; },
            sol::meta_function::subtraction, [](const glm::vec2& a, const glm::vec2& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const glm::vec2& a, const glm::vec2& b) { return a * b; },
                [](const glm::vec2& a, float b) { return a * b; },
                [](float a, const glm::vec2& b) { return a * b; }
            ),
            sol::meta_function::division, sol::overload(
                [](const glm::vec2& a, const glm::vec2& b) { return a / b; },
                [](const glm::vec2& a, float b) { return a / b; }
            ),
            "normalize", [](const glm::vec2& v) { return glm::normalize(v); },
            "length", [](const glm::vec2& v) { return glm::length(v); },
            "dot", [](const glm::vec2& a, const glm::vec2& b) { return glm::dot(a, b); }
        );

        // Register Quaternion type (glm::quat)
        state.new_usertype<glm::quat>("Quaternion",
            sol::constructors<glm::quat(), glm::quat(float, float, float, float)>(),
            "w", &glm::quat::w,
            "x", &glm::quat::x,
            "y", &glm::quat::y,
            "z", &glm::quat::z,
            "fromEulerAngles", [](float pitch, float yaw, float roll) -> glm::quat {
                return glm::quat(glm::vec3(
                    glm::radians(pitch),
                    glm::radians(yaw),
                    glm::radians(roll)
                ));
            },
            "fromAxisAngle", [](const glm::vec3& axis, float angle) -> glm::quat {
                return glm::angleAxis(glm::radians(angle), glm::normalize(axis));
            },
            "getEulerAngles", [](const glm::quat& q) -> glm::vec3 {
                auto eulerRadians = glm::eulerAngles(q);
                return glm::vec3(
                    glm::degrees(eulerRadians.x),
                    glm::degrees(eulerRadians.y),
                    glm::degrees(eulerRadians.z)
                );
            },
            sol::meta_function::multiplication, [](const glm::quat& a, const glm::quat& b) { return a * b; },
            "normalize", [](const glm::quat& q) { return glm::normalize(q); },
            "conjugate", [](const glm::quat& q) { return glm::conjugate(q); },
            "inverse", [](const glm::quat& q) { return glm::inverse(q); },
            "rotateVector", [](const glm::quat& q, const glm::vec3& v) { return q * v; }
        );

        // Register mat4 type (glm::mat4)
        state.new_usertype<glm::mat4>("Matrix4",
            sol::constructors<glm::mat4(), glm::mat4(float)>(),
            "identity", []() { return glm::mat4(1.0f); },
            sol::meta_function::multiplication, sol::overload(
                [](const glm::mat4& a, const glm::mat4& b) { return a * b; },
                [](const glm::mat4& m, const glm::vec4& v) { return m * v; }
            ),
            "translate", [](const glm::mat4& m, const glm::vec3& v) { return glm::translate(m, v); },
            "rotate", [](const glm::mat4& m, float angle, const glm::vec3& axis) {
                return glm::rotate(m, glm::radians(angle), axis);
            },
            "scale", [](const glm::mat4& m, const glm::vec3& v) { return glm::scale(m, v); },
            "inverse", [](const glm::mat4& m) { return glm::inverse(m); },
            "transpose", [](const glm::mat4& m) { return glm::transpose(m); }
        );
    }
}