#include "pch.h"
#include "ValueSerialization.h"
#include "TypeSerializationTable.h"
#include "ShaderStruct.h"
#include "ShaderArray.h"

namespace ShaderLib {

    // ============================================================================
    // BASE TYPE SERIALIZATION - Simple wrappers
    // ============================================================================

    json BaseTypeValueToJson(BaseType type, const BufferValue& value) {
        const auto& info = GetSerializationInfo(type);
        if (!info.SupportsJson()) {
            throw std::runtime_error("Type does not support JSON serialization");
        }
        return info.toJson(value);
    }

    BufferValue BaseTypeValueFromJson(BaseType type, const json& j) {
        const auto& info = GetSerializationInfo(type);
        if (!info.SupportsJson()) {
            throw std::runtime_error("Type does not support JSON deserialization");
        }
        return info.fromJson(j);
    }

    BufferValue ReadBaseTypeFromBuffer(BaseType type, const void* src) {
        if (!src) {
            throw std::runtime_error("Source buffer is null");
        }

        const auto& info = GetSerializationInfo(type);
        if (!info.IsValid()) {
            throw std::runtime_error("Type does not support buffer reading");
        }
        return info.readFromBuffer(src);
    }

    bool WriteBaseTypeToFixedBuffer(BaseType type, void* dst, const BufferValue& value) {
        if (!dst) return false;

        const auto& info = GetSerializationInfo(type);
        if (!info.IsValid()) return false;

        return info.writeToFixedBuffer(dst, value);
    }

    bool WriteBaseTypeToBuffer(BaseType type, std::vector<uint8_t>& dst, const BufferValue& value) {
        const auto& info = GetSerializationInfo(type);
        if (!info.IsValid()) return false;

        return info.writeToBuffer(dst, value);
    }

    bool WriteBaseTypeFromJson(BaseType type, std::vector<uint8_t>& dst, const json& jsonValue) {
        const auto& info = GetSerializationInfo(type);
        if (!info.SupportsJson()) return false;

        return info.writeFromJson(dst, jsonValue);
    }

    // ============================================================================
    // GENERIC VALUE SERIALIZATION
    // ============================================================================

    json BufferValueToJson(const BufferValue& value) {
        size_t index = value.index();
        BaseType type = VariantIndexToBaseType(index);

        // Kompozyty - bezpośrednio przez ich metody
        if (type == BaseType::Struct) {
            return std::get<std::shared_ptr<ShaderStructInstance>>(value)->ToJson();
        }
        else if (type == BaseType::Array) {
            return std::get<std::shared_ptr<ShaderArrayInstance>>(value)->ToJson();
        }
        // Typy bazowe - przez tablicę
        else {
            return BaseTypeValueToJson(type, value);
        }
    }

    BufferValue BufferValueFromJson(const json& j, const ValueTypeInfo& typeInfo) {
        if (typeInfo.IsComposite()) {
            auto instance = typeInfo.compositeType->CreateInstance();
            instance->FromJson(j);
            return ConvertCompositeToBufferValue(instance);
        }
        else if (typeInfo.IsBase()) {
            return BaseTypeValueFromJson(typeInfo.baseType, j);
        }

        throw std::runtime_error("Invalid type info for deserialization");
    }

    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================

    uint32_t GetValueSize(const BufferValue& value) {
        size_t index = value.index();
        BaseType type = VariantIndexToBaseType(index);

        if (type == BaseType::Struct) {
            auto structPtr = std::get<std::shared_ptr<ShaderStructInstance>>(value);
            return structPtr->GetDefinition()->GetSize();
        }
        else if (type == BaseType::Array) {
            auto arrayPtr = std::get<std::shared_ptr<ShaderArrayInstance>>(value);
            return arrayPtr->GetDefinition()->GetSize();
        }
        else {
            return GetBaseTypeInfo(type).size;
        }
    }

    bool WriteValueToBuffer(std::vector<uint8_t>& dst, const BufferValue& value) {
        size_t index = value.index();
        BaseType type = VariantIndexToBaseType(index);

        if (type == BaseType::Struct) {
            auto structPtr = std::get<std::shared_ptr<ShaderStructInstance>>(value);
            const auto& buffer = structPtr->GetRawBuffer();
            dst.insert(dst.end(), buffer.begin(), buffer.end());
            return true;
        }
        else if (type == BaseType::Array) {
            auto arrayPtr = std::get<std::shared_ptr<ShaderArrayInstance>>(value);
            const auto& buffer = arrayPtr->GetRawBuffer();
            dst.insert(dst.end(), buffer.begin(), buffer.end());
            return true;
        }
        else {
            return WriteBaseTypeToBuffer(type, dst, value);
        }
    }

    BufferValue ReadValueFromBuffer(const void* src, const ValueTypeInfo& typeInfo) {
        if (!src) {
            throw std::runtime_error("Source buffer is null");
        }

        if (typeInfo.IsComposite()) {
            auto instance = typeInfo.compositeType->CreateInstance();
            instance->ReadFromBuffer(src);

            if (instance->IsStruct()) {
                return std::static_pointer_cast<ShaderStructInstance>(instance);
            }
            else {
                return std::static_pointer_cast<ShaderArrayInstance>(instance);
            }
        }
        else if (typeInfo.IsBase()) {
            return ReadBaseTypeFromBuffer(typeInfo.baseType, src);
        }

        throw std::runtime_error("Invalid type info for buffer reading");
    }

    BufferValue ConvertCompositeToBufferValue(std::shared_ptr<CompositeTypeInstance> instance) {
        if (instance->IsStruct()) {
            return std::static_pointer_cast<ShaderStructInstance>(instance);
        }
        else if (instance->IsArray()) {
            return std::static_pointer_cast<ShaderArrayInstance>(instance);
        }
        throw std::runtime_error("Unknown composite type");
    }

} // namespace ShaderLib