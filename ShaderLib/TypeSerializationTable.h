#pragma once
#include "ShaderTypes.h"
#include <json.hpp>
#include <vector>

namespace ShaderLib {
    using json = nlohmann::json;

    // ============================================================================
    // BASE TYPE SERIALIZATION FUNCTIONS
    // ============================================================================

    struct BaseTypeSerializationInfo {
        BaseType type;

        // Binary serialization
        BaseTypeValue(*readFromBuffer)(const void* src);
        bool (*writeToFixedBuffer)(void* dst, const BaseTypeValue& value);
        bool (*writeToBuffer)(std::vector<uint8_t>& dst, const BaseTypeValue& value);

        // JSON serialization
        json(*toJson)(const BaseTypeValue& value);
        BaseTypeValue(*fromJson)(const json& j);
        bool (*writeFromJson)(std::vector<uint8_t>& dst, const json& j);

        constexpr bool IsValid() const {
            return type != BaseType::Unknown
                && readFromBuffer != nullptr
                && writeToFixedBuffer != nullptr;
        }

        constexpr bool SupportsJson() const {
            return toJson != nullptr && fromJson != nullptr;
        }
    };

    // ============================================================================
    // SERIALIZATION TABLE
    // ============================================================================

    extern const BaseTypeSerializationInfo BASE_TYPE_SERIALIZATION_TABLE[];

    // Helper function
    constexpr const BaseTypeSerializationInfo& GetSerializationInfo(BaseType type) {
        size_t index = static_cast<size_t>(type);
        return (index < static_cast<size_t>(BaseType::COUNT))
            ? BASE_TYPE_SERIALIZATION_TABLE[index]
            : BASE_TYPE_SERIALIZATION_TABLE[static_cast<size_t>(BaseType::Unknown)];
    }

} // namespace ShaderLib
