#include "pch.h"
#include "ShaderTypes.h"
#include <unordered_map>

namespace ShaderLib {

    // ============================================================================
    // STRING TO TYPE CONVERSION
    // ============================================================================

    BaseType StringToBaseType(const std::string& typeName) {
        static const std::unordered_map<std::string, BaseType> mapping = []() {
            std::unordered_map<std::string, BaseType> map;

            for (size_t i = 0; i < static_cast<size_t>(BaseType::COUNT); ++i) {
                const BaseTypeInfo& info = BASE_TYPE_TABLE[i];
                if (info.IsValid()) {
                    map[info.glslName] = info.type;
                }
            }

            return map;
            }();

        auto it = mapping.find(typeName);
        return (it != mapping.end()) ? it->second : BaseType::Unknown;
    }

    // ============================================================================
    // VARIANT HELPERS
    // ============================================================================

    BaseType VariantIndexToBaseType(size_t index) {
        static constexpr BaseType indexMap[] = {
            BaseType::Bool,      // 0
            BaseType::Float,     // 1
            BaseType::Vec2,      // 2
            BaseType::Vec3,      // 3
            BaseType::Vec4,      // 4
            BaseType::Int,       // 5
            BaseType::IVec2,     // 6
            BaseType::IVec3,     // 7
            BaseType::IVec4,     // 8
            BaseType::UInt,      // 9
            BaseType::UVec2,     // 10
            BaseType::UVec3,     // 11
            BaseType::UVec4,     // 12
            BaseType::Double,    // 13
            BaseType::DVec2,     // 14
            BaseType::DVec3,     // 15
            BaseType::DVec4,     // 16
            BaseType::Mat2,      // 17
            BaseType::Mat3,      // 18
            BaseType::Mat4,      // 19
            BaseType::Struct,    // 20
            BaseType::Array,     // 21
            BaseType::Unknown
        };

        constexpr size_t mapSize = sizeof(indexMap) / sizeof(indexMap[0]);
        return (index < mapSize) ? indexMap[index] : BaseType::Unknown;
    }

    ShaderTypeCategory VariantIndexToCategory(size_t index) {
        // Index 20 = std::shared_ptr<ShaderStruct>
        // Index 21 = std::shared_ptr<ShaderArray>
        constexpr size_t structIndex = 20;
        constexpr size_t arrayIndex = 21;

        if (index == structIndex || index == arrayIndex) {
            return ShaderTypeCategory::Composite;
        }
        else if (index < structIndex) {
            return ShaderTypeCategory::Base;
        }

        return ShaderTypeCategory::Unknown;
    }

    // USUŃ 'inline' i zamień na normalną funkcję:
    BaseType GetBaseTypeFromVariant(const BufferValue& value) {
        size_t index = value.index();
        return VariantIndexToBaseType(index);
    }

} // namespace ShaderLib