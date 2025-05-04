#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <map>
#include <array>
#include <glm/glm.hpp>
#include <shaderc/shaderc.hpp>
#include <sstream>
#include <assert.h>
#include <iostream>
#include "ShaderLib.h"

namespace ShaderLib {

    // Forward declarations
    enum class UniformType;
    struct TypeInfo;
    struct UniformVariable;
    class UBOBuilder;

    // Size alignment helper
    constexpr uint32_t AlignTo(uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    // Helper to automatically infer UniformType from C++ type
    template<typename T>
    struct UniformTypeTraits {
        static constexpr UniformType type = UniformType::Unknown;
        static constexpr uint32_t size = 0;
        static constexpr uint32_t alignment = 0;
        static std::string glslTypeName() { return "unknown"; }
    };

    // Light structure definitions that can be used in both C++ and shader code
    struct DirectionalLight {
        alignas(16) glm::vec3 direction;
        alignas(16) glm::vec4 color; // w is intensity

        // Generate GLSL struct definition
        static std::string GetGLSLDefinition() {
            return "struct DirectionalLight {\n"
                "    vec3 direction;\n"
                "    vec4 color; // w is intensity\n"
                "};\n\n";
        }
    };

    struct PointLight {
        alignas(16) glm::vec3 position;
        float radius;
        alignas(16) glm::vec4 color; // w is intensity

        // Generate GLSL struct definition
        static std::string GetGLSLDefinition() {
            return "struct PointLight {\n"
                "    vec3 position;\n"
                "    float radius;\n"
                "    vec4 color; // w is intensity\n"
                "};\n\n";
        }
    };

    struct SpotLight {
        alignas(16) glm::vec3 position;
        float innerCutoff;
        alignas(16) glm::vec3 direction;
        float outerCutoff;
        alignas(16) glm::vec4 color; // w is intensity
        float range;
        float padding[3]; // Explicit padding for consistent memory layout

        // Generate GLSL struct definition
        static std::string GetGLSLDefinition() {
            return "struct SpotLight {\n"
                "    vec3 position;\n"
                "    float innerCutoff;\n"
                "    vec3 direction;\n"
                "    float outerCutoff;\n"
                "    vec4 color; // w is intensity\n"
                "    float range;\n"
                "    float padding[3]; // Explicit padding\n"
                "};\n\n";
        }
    };

    // UBO field descriptor for declarative UBO definition
    struct UBOField {
        std::string name;
        UniformType type;
        uint32_t arraySize;
        std::string typeName;
        std::string comment;
        bool isStruct;

        UBOField(const std::string& fieldName, UniformType fieldType, const std::string& fieldComment = "")
            : name(fieldName), type(fieldType), arraySize(0), typeName(""), comment(fieldComment), isStruct(false) {
        }

        UBOField(const std::string& fieldName, UniformType fieldType, uint32_t fieldArraySize, const std::string& fieldComment = "")
            : name(fieldName), type(fieldType), arraySize(fieldArraySize), typeName(""), comment(fieldComment), isStruct(false) {
        }

        UBOField(const std::string& fieldName, const std::string& structTypeName, const std::string& fieldComment = "")
            : name(fieldName), type(UniformType::Struct), arraySize(0), typeName(structTypeName), comment(fieldComment), isStruct(true) {
        }

        UBOField(const std::string& fieldName, const std::string& structTypeName, uint32_t fieldArraySize, const std::string& fieldComment = "")
            : name(fieldName), type(UniformType::Array), arraySize(fieldArraySize), typeName(structTypeName), comment(fieldComment), isStruct(true) {
        }
    };

    // Template specializations for common types used in UBOs
    template<> struct UniformTypeTraits<bool> {
        static constexpr UniformType type = UniformType::Bool;
        static constexpr uint32_t size = sizeof(bool);
        static constexpr uint32_t alignment = 4; // Bool in STD140 is 4 bytes
        static std::string glslTypeName() { return "bool"; }
    };

    template<> struct UniformTypeTraits<float> {
        static constexpr UniformType type = UniformType::Float;
        static constexpr uint32_t size = sizeof(float);
        static constexpr uint32_t alignment = 4;
        static std::string glslTypeName() { return "float"; }
    };

    template<> struct UniformTypeTraits<glm::vec2> {
        static constexpr UniformType type = UniformType::Vec2;
        static constexpr uint32_t size = sizeof(glm::vec2);
        static constexpr uint32_t alignment = 8;
        static std::string glslTypeName() { return "vec2"; }
    };

    template<> struct UniformTypeTraits<glm::vec3> {
        static constexpr UniformType type = UniformType::Vec3;
        static constexpr uint32_t size = sizeof(glm::vec3);
        static constexpr uint32_t alignment = 16; // Vec3 in STD140 has alignment of 16
        static std::string glslTypeName() { return "vec3"; }
    };

    template<> struct UniformTypeTraits<glm::vec4> {
        static constexpr UniformType type = UniformType::Vec4;
        static constexpr uint32_t size = sizeof(glm::vec4);
        static constexpr uint32_t alignment = 16;
        static std::string glslTypeName() { return "vec4"; }
    };

    template<> struct UniformTypeTraits<glm::mat2> {
        static constexpr UniformType type = UniformType::Mat2;
        static constexpr uint32_t size = sizeof(glm::mat2);
        static constexpr uint32_t alignment = 8; // Each column is vec2 (8-byte aligned)
        static std::string glslTypeName() { return "mat2"; }
    };

    template<> struct UniformTypeTraits<glm::mat3> {
        static constexpr UniformType type = UniformType::Mat3;
        static constexpr uint32_t size = sizeof(glm::mat3);
        static constexpr uint32_t alignment = 16; // Each column is vec3 (16-byte aligned)
        static std::string glslTypeName() { return "mat3"; }
    };

    template<> struct UniformTypeTraits<glm::mat4> {
        static constexpr UniformType type = UniformType::Mat4;
        static constexpr uint32_t size = sizeof(glm::mat4);
        static constexpr uint32_t alignment = 16; // Each column is vec4 (16-byte aligned)
        static std::string glslTypeName() { return "mat4"; }
    };

    template<> struct UniformTypeTraits<int> {
        static constexpr UniformType type = UniformType::Int;
        static constexpr uint32_t size = sizeof(int);
        static constexpr uint32_t alignment = 4;
        static std::string glslTypeName() { return "int"; }
    };

    template<> struct UniformTypeTraits<glm::ivec2> {
        static constexpr UniformType type = UniformType::IVec2;
        static constexpr uint32_t size = sizeof(glm::ivec2);
        static constexpr uint32_t alignment = 8;
        static std::string glslTypeName() { return "ivec2"; }
    };

    template<> struct UniformTypeTraits<glm::ivec3> {
        static constexpr UniformType type = UniformType::IVec3;
        static constexpr uint32_t size = sizeof(glm::ivec3);
        static constexpr uint32_t alignment = 16; // IVec3 in STD140 has alignment of 16
        static std::string glslTypeName() { return "ivec3"; }
    };

    template<> struct UniformTypeTraits<glm::ivec4> {
        static constexpr UniformType type = UniformType::IVec4;
        static constexpr uint32_t size = sizeof(glm::ivec4);
        static constexpr uint32_t alignment = 16;
        static std::string glslTypeName() { return "ivec4"; }
    };

    template<> struct UniformTypeTraits<unsigned int> {
        static constexpr UniformType type = UniformType::UInt;
        static constexpr uint32_t size = sizeof(unsigned int);
        static constexpr uint32_t alignment = 4;
        static std::string glslTypeName() { return "uint"; }
    };

    template<> struct UniformTypeTraits<glm::uvec2> {
        static constexpr UniformType type = UniformType::UVec2;
        static constexpr uint32_t size = sizeof(glm::uvec2);
        static constexpr uint32_t alignment = 8;
        static std::string glslTypeName() { return "uvec2"; }
    };

    template<> struct UniformTypeTraits<glm::uvec3> {
        static constexpr UniformType type = UniformType::UVec3;
        static constexpr uint32_t size = sizeof(glm::uvec3);
        static constexpr uint32_t alignment = 16; // UVec3 in STD140 has alignment of 16
        static std::string glslTypeName() { return "uvec3"; }
    };

    template<> struct UniformTypeTraits<glm::uvec4> {
        static constexpr UniformType type = UniformType::UVec4;
        static constexpr uint32_t size = sizeof(glm::uvec4);
        static constexpr uint32_t alignment = 16;
        static std::string glslTypeName() { return "uvec4"; }
    };

    template<> struct UniformTypeTraits<double> {
        static constexpr UniformType type = UniformType::Double;
        static constexpr uint32_t size = sizeof(double);
        static constexpr uint32_t alignment = 8;
        static std::string glslTypeName() { return "double"; }
    };

    template<> struct UniformTypeTraits<DirectionalLight> {
        static constexpr UniformType type = UniformType::Struct;
        static constexpr uint32_t size = sizeof(DirectionalLight);
        static constexpr uint32_t alignment = 16;
        static std::string glslTypeName() { return "DirectionalLight"; }
    };

    template<> struct UniformTypeTraits<PointLight> {
        static constexpr UniformType type = UniformType::Struct;
        static constexpr uint32_t size = sizeof(PointLight);
        static constexpr uint32_t alignment = 16;
        static std::string glslTypeName() { return "PointLight"; }
    };

    template<> struct UniformTypeTraits<SpotLight> {
        static constexpr UniformType type = UniformType::Struct;
        static constexpr uint32_t size = sizeof(SpotLight);
        static constexpr uint32_t alignment = 16;
        static std::string glslTypeName() { return "SpotLight"; }
    };

    // Template for array types
    template<typename T, size_t N>
    struct UniformTypeTraits<std::array<T, N>> {
        static constexpr UniformType type = UniformType::Array;
        static constexpr uint32_t size = N * AlignTo(sizeof(T), UniformTypeTraits<T>::alignment);
        static constexpr uint32_t alignment = UniformTypeTraits<T>::alignment;
        static std::string glslTypeName() {
            return UniformTypeTraits<T>::glslTypeName() + "[" + std::to_string(N) + "]";
        }
    };

    // Helper to get type info for a struct type by name
    TypeInfo GetStructTypeInfo(const std::string& structName);

    // UBO Builder - fluid interface for constructing UBOs
    class UBOBuilder {
    private:
        std::string name;
        uint32_t set;
        uint32_t binding;
        std::vector<UBOField> fields;
        std::vector<std::string> structDefinitions;

    public:
        UBOBuilder(const std::string& uboName, uint32_t descriptorSet, uint32_t bindingPoint)
            : name(uboName), set(descriptorSet), binding(bindingPoint) {
        }

        // Add a field of a basic type
        template<typename T>
        UBOBuilder& AddField(const std::string& fieldName, const std::string& comment = "") {
            fields.emplace_back(fieldName, UniformTypeTraits<T>::type, comment);
            return *this;
        }

        // Add a field that's a struct
        template<typename T>
        UBOBuilder& AddStructField(const std::string& fieldName, const std::string& comment = "") {
            fields.emplace_back(fieldName, UniformTypeTraits<T>::glslTypeName(), comment);

            // Add struct definition if it has one
            if constexpr (requires { T::GetGLSLDefinition(); }) {
                structDefinitions.push_back(T::GetGLSLDefinition());
            }

            return *this;
        }

        // Add an array field
        template<typename T>
        UBOBuilder& AddArrayField(const std::string& fieldName, uint32_t count, const std::string& comment = "") {
            if constexpr (std::is_same_v<T, DirectionalLight> ||
                std::is_same_v<T, PointLight> ||
                std::is_same_v<T, SpotLight>) {
                // This is a struct array
                fields.emplace_back(fieldName, UniformTypeTraits<T>::glslTypeName(), count, comment);

                // Add struct definition if needed
                structDefinitions.push_back(T::GetGLSLDefinition());
            }
            else {
                // This is a basic type array
                auto field = UBOField(fieldName, UniformTypeTraits<T>::type, count, comment);
                field.typeName = UniformTypeTraits<T>::glslTypeName();
                fields.push_back(field);
            }
            return *this;
        }

        // Build and return the UBO object
        UniformBufferObject Build();

        // Generate GLSL code for this UBO
        std::string GenerateGLSL();

        // Get struct definitions needed for this UBO
        const std::vector<std::string>& GetStructDefinitions() const {
            return structDefinitions;
        }
    };

    // Registry for UBO definitions - singleton to access predefined UBOs
    class UBORegistry {
    private:
        std::unordered_map<std::string, UniformBufferObject> ubos;

        UBORegistry() = default;

    public:
        static UBORegistry& Get() {
            static UBORegistry instance;
            return instance;
        }

        // Register a UBO definition
        void RegisterUBO(const UniformBufferObject& ubo);

        // Get a registered UBO by name
        const UniformBufferObject* GetUBO(const std::string& name) const;

        std::string GenerateGLSL(const std::string& uboName) const;

        // Initialize standard UBOs
        void InitializeStandardUBOs();

        // Get GlobalUBO definition
        static UniformBufferObject CreateGlobalUBO();

        // Get ObjectUBO definition
        static UniformBufferObject CreateObjectUBO();
    };

} // namespace ShaderLib