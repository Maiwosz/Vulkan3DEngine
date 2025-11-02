#pragma once
#include <json.hpp>
#include "ShaderTypes.h"
#include <memory>
#include <vector>

namespace ShaderLib {
    using json = nlohmann::json;

    // ============================================================================
    // VALUE TYPE INFO - Helper for deserialization
    // ============================================================================

    struct ValueTypeInfo {
        BaseType baseType = BaseType::Unknown;
        std::shared_ptr<const CompositeTypeDefinition> compositeType = nullptr; // było CompositeType

        bool IsBase() const { return baseType != BaseType::Unknown && !compositeType; }
        bool IsComposite() const { return compositeType != nullptr; }
    };

    // ============================================================================
    // BASE TYPE VALUE SERIALIZATION - JSON
    // ============================================================================

    // Serialize base type value to JSON
    json BaseTypeValueToJson(BaseType type, const BufferValue& value);

    // Deserialize base type value from JSON
    BufferValue BaseTypeValueFromJson(BaseType type, const json& j);

    // ============================================================================
    // BASE TYPE VALUE SERIALIZATION - BINARY
    // ============================================================================

    // Write base type value to binary buffer (append mode)
    bool WriteBaseTypeToBuffer(BaseType type, std::vector<uint8_t>& dst, const BufferValue& value);

    // Write base type value directly from JSON to binary buffer (append mode)
    bool WriteBaseTypeFromJson(BaseType type, std::vector<uint8_t>& dst, const json& jsonValue);

    // Read base type from fixed buffer location
    BufferValue ReadBaseTypeFromBuffer(BaseType type, const void* src);

    // Write base type to fixed buffer location
    bool WriteBaseTypeToFixedBuffer(BaseType type, void* dst, const BufferValue& value);

    // ============================================================================
    // GENERIC VALUE SERIALIZATION
    // ============================================================================

    // Serialize any BufferValue (base type or composite) to JSON
    json BufferValueToJson(const BufferValue& value);

    // Deserialize BufferValue from JSON (requires type information)
    BufferValue BufferValueFromJson(const json& j, const ValueTypeInfo& typeInfo);

    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================

    // Get size of a value in bytes
    uint32_t GetValueSize(const BufferValue& value);

    // Write BufferValue to binary buffer (append mode)
    bool WriteValueToBuffer(std::vector<uint8_t>& dst, const BufferValue& value);

    // Read BufferValue from binary buffer
    BufferValue ReadValueFromBuffer(const void* src, const ValueTypeInfo& typeInfo);

    BufferValue ConvertCompositeToBufferValue(std::shared_ptr<CompositeTypeInstance> instance);

} // namespace ShaderLib