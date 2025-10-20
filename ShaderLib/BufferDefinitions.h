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
    class BufferBuilder;

    // Size alignment helpers for different standards
    constexpr uint32_t AlignTo(uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    // Get alignment for array elements based on layout standard
    inline uint32_t GetArrayElementAlignment(uint32_t baseAlignment, LayoutStandard standard) {
        if (standard == LayoutStandard::Std140) {
            // std140: arrays are aligned to 16 bytes or base alignment, whichever is greater
            return std::max(16u, baseAlignment);
        }
        else {
            // std430: arrays use base type alignment
            return baseAlignment;
        }
    }

    // Get type info adjusted for layout standard
    inline TypeInfo GetTypeInfoForStandard(UniformType type, LayoutStandard standard);

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
        glm::vec3 direction;
        float padding1;
        glm::vec4 color;

        static std::string GetGLSLDefinition() {
            return "struct DirectionalLight {\n"
                "    vec3 direction;\n"
                "    vec4 color; // w is intensity\n"
                "};\n\n";
        }
    };

    struct PointLight {
        glm::vec3 position;
        float radius;
        glm::vec4 color;

        static std::string GetGLSLDefinition() {
            return "struct PointLight {\n"
                "    vec3 position;\n"
                "    float radius;\n"
                "    vec4 color; // w is intensity\n"
                "};\n\n";
        }
    };

    struct SpotLight {
        glm::vec3 position;
        float innerCutoff;
        glm::vec3 direction;
        float outerCutoff;
        glm::vec4 color;
        float range;
        float padding[3];

        static std::string GetGLSLDefinition() {
            return "struct SpotLight {\n"
                "    vec3 position;\n"
                "    float innerCutoff;\n"
                "    vec3 direction;\n"
                "    float outerCutoff;\n"
                "    vec4 color; // w is intensity\n"
                "    float range;\n"
                "    float padding[3];\n"
                "};\n\n";
        }
    };

    // Buffer field descriptor for declarative buffer definition
    struct BufferField {
        std::string name;
        UniformType type;
        uint32_t arraySize;
        std::string typeName;
        std::string comment;
        bool isStruct;

        BufferField(const std::string& fieldName, UniformType fieldType, const std::string& fieldComment = "")
            : name(fieldName), type(fieldType), arraySize(0), typeName(""), comment(fieldComment), isStruct(false) {
        }

        BufferField(const std::string& fieldName, UniformType fieldType, uint32_t fieldArraySize, const std::string& fieldComment = "")
            : name(fieldName), type(fieldType), arraySize(fieldArraySize), typeName(""), comment(fieldComment), isStruct(false) {
        }

        BufferField(const std::string& fieldName, const std::string& structTypeName, const std::string& fieldComment = "")
            : name(fieldName), type(UniformType::Struct), arraySize(0), typeName(structTypeName), comment(fieldComment), isStruct(true) {
        }

        BufferField(const std::string& fieldName, const std::string& structTypeName, uint32_t fieldArraySize, const std::string& fieldComment = "")
            : name(fieldName), type(UniformType::Array), arraySize(fieldArraySize), typeName(structTypeName), comment(fieldComment), isStruct(true) {
        }
    };

    // Template specializations for common types
    template<> struct UniformTypeTraits<bool> {
        static constexpr UniformType type = UniformType::Bool;
        static constexpr uint32_t size = sizeof(bool);
        static constexpr uint32_t alignment = 4;
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
        static constexpr uint32_t alignment = 16;
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
        static constexpr uint32_t alignment = 8;
        static std::string glslTypeName() { return "mat2"; }
    };

    template<> struct UniformTypeTraits<glm::mat3> {
        static constexpr UniformType type = UniformType::Mat3;
        static constexpr uint32_t size = sizeof(glm::mat3);
        static constexpr uint32_t alignment = 16;
        static std::string glslTypeName() { return "mat3"; }
    };

    template<> struct UniformTypeTraits<glm::mat4> {
        static constexpr UniformType type = UniformType::Mat4;
        static constexpr uint32_t size = sizeof(glm::mat4);
        static constexpr uint32_t alignment = 16;
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
        static constexpr uint32_t alignment = 16;
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
        static constexpr uint32_t alignment = 16;
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

    // Universal Buffer Builder - works for both UBO and SSBO
    class BufferBuilder {
    private:
        std::string name;
        uint32_t set;
        uint32_t binding;
        BufferType bufferType;
        LayoutStandard layoutStandard;
        std::vector<BufferField> fields;
        std::vector<std::string> structDefinitions;

    public:
        // Constructor for uniform buffer (std140)
        BufferBuilder(const std::string& bufferName, uint32_t descriptorSet, uint32_t bindingPoint)
            : name(bufferName), set(descriptorSet), binding(bindingPoint),
            bufferType(BufferType::Uniform), layoutStandard(LayoutStandard::Std140) {
        }

        // Constructor with explicit buffer type
        BufferBuilder(const std::string& bufferName, uint32_t descriptorSet, uint32_t bindingPoint,
            BufferType type, LayoutStandard standard = LayoutStandard::Std140)
            : name(bufferName), set(descriptorSet), binding(bindingPoint),
            bufferType(type), layoutStandard(standard) {
            // Auto-select standard based on buffer type if not explicitly set
            if (type == BufferType::Storage && standard == LayoutStandard::Std140) {
                layoutStandard = LayoutStandard::Std430;
            }
        }

        // Fluent setters
        BufferBuilder& SetBufferType(BufferType type) {
            bufferType = type;
            // Auto-adjust standard if needed
            if (type == BufferType::Storage && layoutStandard == LayoutStandard::Std140) {
                layoutStandard = LayoutStandard::Std430;
            }
            return *this;
        }

        BufferBuilder& SetLayoutStandard(LayoutStandard standard) {
            layoutStandard = standard;
            return *this;
        }

        // Add a field of a basic type
        template<typename T>
        BufferBuilder& AddField(const std::string& fieldName, const std::string& comment = "") {
            fields.emplace_back(fieldName, UniformTypeTraits<T>::type, comment);
            return *this;
        }

        // Add a field that's a struct
        template<typename T>
        BufferBuilder& AddStructField(const std::string& fieldName, const std::string& comment = "") {
            fields.emplace_back(fieldName, UniformTypeTraits<T>::glslTypeName(), comment);

            if constexpr (requires { T::GetGLSLDefinition(); }) {
                structDefinitions.push_back(T::GetGLSLDefinition());
            }

            return *this;
        }

        // Add an array field
        template<typename T>
        BufferBuilder& AddArrayField(const std::string& fieldName, uint32_t count, const std::string& comment = "") {
            if constexpr (std::is_same_v<T, DirectionalLight> ||
                std::is_same_v<T, PointLight> ||
                std::is_same_v<T, SpotLight>) {
                fields.emplace_back(fieldName, UniformTypeTraits<T>::glslTypeName(), count, comment);
                structDefinitions.push_back(T::GetGLSLDefinition());
            }
            else {
                auto field = BufferField(fieldName, UniformTypeTraits<T>::type, count, comment);
                field.typeName = UniformTypeTraits<T>::glslTypeName();
                fields.push_back(field);
            }
            return *this;
        }

        // Build and return the buffer object
        BufferObject Build();

        // Generate GLSL code for this buffer
        std::string GenerateGLSL();

        const std::vector<std::string>& GetStructDefinitions() const {
            return structDefinitions;
        }
    };

    // Aliases for backward compatibility
    using UBOBuilder = BufferBuilder;

    // Registry for buffer definitions
    class BufferRegistry {
    private:
        std::unordered_map<std::string, BufferObject> buffers;

        BufferRegistry() = default;

    public:
        static BufferRegistry& Get() {
            static BufferRegistry instance;
            return instance;
        }

        void RegisterBuffer(const BufferObject& buffer);
        const BufferObject* GetBuffer(const std::string& name) const;
        std::string GenerateGLSL(const std::string& bufferName) const;

        void InitializeStandardBuffers();

        // Standard buffer creators
        static BufferObject CreateGlobalUBO();
        static BufferObject CreateObjectUBO();
    };

    // Alias for backward compatibility
    using UBORegistry = BufferRegistry;

    inline bool IsLayoutCompatible(BufferType bufferType, LayoutStandard layout) {
        if (bufferType == BufferType::Uniform) {
            return layout == LayoutStandard::Std140;
        }
        // Storage buffers support both, but std430 is preferred
        return true;
    }

} // namespace ShaderLib