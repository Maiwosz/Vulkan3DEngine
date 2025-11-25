#pragma once
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include <variant>
#include <memory>
#include <vector>
#include <json.hpp>

using json = nlohmann::json;

namespace ShaderLib {

    // ============================================================================
    // BASE TYPE SYSTEM
    // ============================================================================

    enum class BaseType {
        // Scalar types
        Bool, Float, Int, UInt, Double,

        // Float vectors
        Vec2, Vec3, Vec4,

        // Integer vectors
        IVec2, IVec3, IVec4,

        // Unsigned integer vectors
        UVec2, UVec3, UVec4,

        // Double vectors
        DVec2, DVec3, DVec4,

        // Matrices
        Mat2, Mat3, Mat4,

        // Atomic types
        AtomicUInt,

        Unknown, COUNT
    };

    // ============================================================================
    // LAYOUT STANDARDS
    // ============================================================================

    enum class LayoutStandard {
        Std140,     // For uniform buffers (UBO)
        Std430,     // For storage buffers (SSBO)
        Packed      // Tight packing (no padding)
    };

    // ============================================================================
    // TYPE FLAGS
    // ============================================================================

    enum class TypeFlags : uint32_t {
        None = 0,
        Scalar = 1 << 0,
        Vector = 1 << 1,
        Matrix = 1 << 2,
        Atomic = 1 << 3,
        Floating = 1 << 4,
        Integer = 1 << 5,
        Unsigned = 1 << 6,
        Double = 1 << 7
    };

    inline constexpr TypeFlags operator|(TypeFlags a, TypeFlags b) {
        return static_cast<TypeFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline constexpr TypeFlags operator&(TypeFlags a, TypeFlags b) {
        return static_cast<TypeFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline constexpr bool HasFlag(TypeFlags flags, TypeFlags flag) {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
    }

    // ============================================================================
    // BASE TYPE INFORMATION
    // ============================================================================

    struct BaseTypeInfo {
        BaseType type;
        uint32_t size;
        uint32_t alignmentStd140;
        uint32_t alignmentStd430;
        TypeFlags flags;
        uint32_t components;
        const char* glslName;

        constexpr uint32_t GetAlignment(LayoutStandard standard) const {
            switch (standard) {
            case LayoutStandard::Std140: return alignmentStd140;
            case LayoutStandard::Std430: return alignmentStd430;
            case LayoutStandard::Packed: return 1;
            default: return alignmentStd140;
            }
        }

        constexpr bool IsValid() const {
            return type != BaseType::Unknown && size > 0;
        }

        constexpr bool IsScalar() const { return HasFlag(flags, TypeFlags::Scalar); }
        constexpr bool IsVector() const { return HasFlag(flags, TypeFlags::Vector); }
        constexpr bool IsMatrix() const { return HasFlag(flags, TypeFlags::Matrix); }
        constexpr bool IsFloating() const { return HasFlag(flags, TypeFlags::Floating); }
        constexpr bool IsInteger() const { return HasFlag(flags, TypeFlags::Integer); }
        constexpr bool IsUnsigned() const { return HasFlag(flags, TypeFlags::Unsigned); }
        constexpr bool IsAtomic() const { return HasFlag(flags, TypeFlags::Atomic); }
        constexpr bool IsDouble() const { return HasFlag(flags, TypeFlags::Double); }
    };

    // ============================================================================
    // TYPE INFORMATION TABLE
    // ============================================================================

#define TF TypeFlags
#define S TF::Scalar
#define V TF::Vector
#define M TF::Matrix
#define F TF::Floating
#define I TF::Integer
#define U TF::Unsigned
#define D TF::Double
#define A TF::Atomic

    constexpr BaseTypeInfo BASE_TYPE_TABLE[] = {
        {BaseType::Bool,     sizeof(bool),        4,   4,   S,                 1,  "bool"},
        {BaseType::Float,    sizeof(float),       4,   4,   S | F,             1,  "float"},
        {BaseType::Int,      sizeof(int32_t),     4,   4,   S | I,             1,  "int"},
        {BaseType::UInt,     sizeof(uint32_t),    4,   4,   S | I | U,         1,  "uint"},
        {BaseType::Double,   sizeof(double),      8,   8,   S | F | D,         1,  "double"},

        {BaseType::Vec2,     sizeof(glm::vec2),   8,   8,   V | F,             2,  "vec2"},
        {BaseType::Vec3,     sizeof(glm::vec3),   16,  16,  V | F,             3,  "vec3"},
        {BaseType::Vec4,     sizeof(glm::vec4),   16,  16,  V | F,             4,  "vec4"},

        {BaseType::IVec2,    sizeof(glm::ivec2),  8,   8,   V | I,             2,  "ivec2"},
        {BaseType::IVec3,    sizeof(glm::ivec3),  16,  16,  V | I,             3,  "ivec3"},
        {BaseType::IVec4,    sizeof(glm::ivec4),  16,  16,  V | I,             4,  "ivec4"},

        {BaseType::UVec2,    sizeof(glm::uvec2),  8,   8,   V | I | U,         2,  "uvec2"},
        {BaseType::UVec3,    sizeof(glm::uvec3),  16,  16,  V | I | U,         3,  "uvec3"},
        {BaseType::UVec4,    sizeof(glm::uvec4),  16,  16,  V | I | U,         4,  "uvec4"},

        {BaseType::DVec2,    sizeof(glm::dvec2),  16,  16,  V | F | D,         2,  "dvec2"},
        {BaseType::DVec3,    sizeof(glm::dvec3),  32,  32,  V | F | D,         3,  "dvec3"},
        {BaseType::DVec4,    sizeof(glm::dvec4),  32,  32,  V | F | D,         4,  "dvec4"},

        {BaseType::Mat2,     sizeof(glm::mat2),   16,  8,   M | F,             4,  "mat2"},
        {BaseType::Mat3,     sizeof(glm::mat3),   16,  16,  M | F,             9,  "mat3"},
        {BaseType::Mat4,     sizeof(glm::mat4),   16,  16,  M | F,             16, "mat4"},

        {BaseType::AtomicUInt, sizeof(uint32_t),  4,   4,   S | I | U | A,     1,  "atomic_uint"},

        {BaseType::Unknown,  0,                   0,   0,   TF::None,          0,  "unknown"},
    };

#undef TF
#undef S
#undef V
#undef M
#undef F
#undef I
#undef U
#undef D
#undef A

    // ============================================================================
    // C++ TYPE VARIANT - For buffer values
    // ============================================================================

    using BaseTypeValue = std::variant<
        bool,
        float,
        glm::vec2, glm::vec3, glm::vec4,
        int32_t,
        glm::ivec2, glm::ivec3, glm::ivec4,
        uint32_t,
        glm::uvec2, glm::uvec3, glm::uvec4,
        double,
        glm::dvec2, glm::dvec3, glm::dvec4,
        glm::mat2, glm::mat3, glm::mat4
    >;

    // ============================================================================
    // TYPE TRAITS
    // ============================================================================

    template<typename T>
    struct BaseTypeTraits {
        static constexpr bool supported = false;
    };

#define DEFINE_BASE_TYPE_TRAITS(CppType, EnumValue) \
    template<> \
    struct BaseTypeTraits<CppType> { \
        static constexpr bool supported = true; \
        static constexpr BaseType type = EnumValue; \
        using CppType_t = CppType; \
        static constexpr const BaseTypeInfo& GetInfo() { \
            return BASE_TYPE_TABLE[static_cast<size_t>(EnumValue)]; \
        } \
    };

    DEFINE_BASE_TYPE_TRAITS(bool, BaseType::Bool)
        DEFINE_BASE_TYPE_TRAITS(float, BaseType::Float)
        DEFINE_BASE_TYPE_TRAITS(int32_t, BaseType::Int)
        DEFINE_BASE_TYPE_TRAITS(uint32_t, BaseType::UInt)
        DEFINE_BASE_TYPE_TRAITS(double, BaseType::Double)
        DEFINE_BASE_TYPE_TRAITS(glm::vec2, BaseType::Vec2)
        DEFINE_BASE_TYPE_TRAITS(glm::vec3, BaseType::Vec3)
        DEFINE_BASE_TYPE_TRAITS(glm::vec4, BaseType::Vec4)
        DEFINE_BASE_TYPE_TRAITS(glm::ivec2, BaseType::IVec2)
        DEFINE_BASE_TYPE_TRAITS(glm::ivec3, BaseType::IVec3)
        DEFINE_BASE_TYPE_TRAITS(glm::ivec4, BaseType::IVec4)
        DEFINE_BASE_TYPE_TRAITS(glm::uvec2, BaseType::UVec2)
        DEFINE_BASE_TYPE_TRAITS(glm::uvec3, BaseType::UVec3)
        DEFINE_BASE_TYPE_TRAITS(glm::uvec4, BaseType::UVec4)
        DEFINE_BASE_TYPE_TRAITS(glm::dvec2, BaseType::DVec2)
        DEFINE_BASE_TYPE_TRAITS(glm::dvec3, BaseType::DVec3)
        DEFINE_BASE_TYPE_TRAITS(glm::dvec4, BaseType::DVec4)
        DEFINE_BASE_TYPE_TRAITS(glm::mat2, BaseType::Mat2)
        DEFINE_BASE_TYPE_TRAITS(glm::mat3, BaseType::Mat3)
        DEFINE_BASE_TYPE_TRAITS(glm::mat4, BaseType::Mat4)

#undef DEFINE_BASE_TYPE_TRAITS

        // ============================================================================
        // UTILITY FUNCTIONS
        // ============================================================================

        constexpr const BaseTypeInfo& GetBaseTypeInfo(BaseType type) {
        size_t index = static_cast<size_t>(type);
        return (index < static_cast<size_t>(BaseType::COUNT))
            ? BASE_TYPE_TABLE[index]
            : BASE_TYPE_TABLE[static_cast<size_t>(BaseType::Unknown)];
    }

    inline const char* BaseTypeToString(BaseType type) {
        return GetBaseTypeInfo(type).glslName;
    }

    BaseType StringToBaseType(const std::string& typeName);

    constexpr uint32_t AlignTo(uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    constexpr uint32_t GetArrayElementAlignment(uint32_t baseAlignment, LayoutStandard standard) {
        return (standard == LayoutStandard::Std140 && baseAlignment < 16) ? 16 : baseAlignment;
    }

    template<typename T>
    constexpr BaseType GetBaseTypeOf() {
        if constexpr (BaseTypeTraits<T>::supported) {
            return BaseTypeTraits<T>::type;
        }
        return BaseType::Unknown;
    }

    template<typename T>
    constexpr bool IsBaseTypeSupported() {
        return BaseTypeTraits<T>::supported;
    }

    // ============================================================================
    // VARIANT HELPERS
    // ============================================================================

    BaseType VariantIndexToBaseType(size_t index);
    BaseType GetBaseTypeFromVariant(const BaseTypeValue& value);

} // namespace ShaderLib
