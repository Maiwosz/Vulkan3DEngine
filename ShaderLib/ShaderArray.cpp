#include "pch.h"
#include "ShaderArray.h"
#include "ShaderStruct.h"
#include <sstream>
#include <cstring>
#include <stdexcept>
#include "TypeSerializationTable.h"
#include "Serialization.h"
#include "ValueSerialization.h"

namespace ShaderLib {

    // ============================================================================
    // SHADER ARRAY DEFINITION - IMPLEMENTATION
    // ============================================================================

    ShaderArrayDefinition::ShaderArrayDefinition(BaseType elemType, uint32_t count,
        LayoutStandard standard)
        : elementBaseType(elemType)
        , elementComposite(nullptr)
        , arrayCount(count)
        , layoutStandard(standard) {

        if (count == 0) {
            throw std::invalid_argument("Array count must be greater than 0");
        }

        const BaseTypeInfo& info = GetBaseTypeInfo(elementBaseType);
        if (!info.IsValid()) {
            throw std::invalid_argument("Invalid base type for array");
        }
        if (info.IsComposite()) {
            throw std::invalid_argument("Use composite constructor for struct/array types");
        }

        elementSize = info.size;
        uint32_t baseAlignment = info.GetAlignment(layoutStandard);
        alignment = GetArrayElementAlignment(baseAlignment, layoutStandard);
        elementStride = AlignTo(elementSize, alignment);
        totalSize = elementStride * arrayCount;
    }

    ShaderArrayDefinition::ShaderArrayDefinition(
        std::shared_ptr<const CompositeTypeDefinition> elemType,
        uint32_t count,
        LayoutStandard standard)
        : elementBaseType(BaseType::Unknown)
        , elementComposite(elemType)
        , arrayCount(count)
        , layoutStandard(standard) {

        if (count == 0) {
            throw std::invalid_argument("Array count must be greater than 0");
        }
        if (!elementComposite) {
            throw std::invalid_argument("Composite element type cannot be null");
        }

        elementSize = elementComposite->GetSize();
        alignment = GetArrayElementAlignment(elementComposite->GetAlignment(), layoutStandard);
        elementStride = AlignTo(elementSize, alignment);
        totalSize = elementStride * arrayCount;
    }

    std::string ShaderArrayDefinition::GetTypeName() const {
        std::stringstream ss;
        if (IsCompositeElement()) {
            ss << elementComposite->GetTypeName();
        }
        else {
            ss << BaseTypeToString(elementBaseType);
        }
        ss << "[" << arrayCount << "]";
        return ss.str();
    }

    std::string ShaderArrayDefinition::GenerateGLSL() const {
        // Arrays are part of variable declaration, not separate types
        // Return only element type information
        if (IsCompositeElement()) {
            return elementComposite->GenerateGLSL();
        }
        return "";
    }

    json ShaderArrayDefinition::ToJson() const {
        json result;
        result["compositeType"] = "array";  // Type discriminator
        result["count"] = arrayCount;
        result["layoutStandard"] = layoutStandard;

        if (IsCompositeElement()) {
            result["elementComposite"] = elementComposite->ToJson();
        }
        else {
            result["elementType"] = elementBaseType;
        }

        return result;
    }

    std::shared_ptr<CompositeTypeInstance> ShaderArrayDefinition::CreateInstance() const
    {
        return std::static_pointer_cast<CompositeTypeInstance>(
            std::make_shared<ShaderArrayInstance>(shared_from_this())
        );
    }

    std::shared_ptr<ShaderArrayDefinition> ShaderArrayDefinition::FromJson(const json& j) {
        uint32_t arrayCount = j.at("count").get<uint32_t>();
        LayoutStandard standard = j.at("layoutStandard").get<LayoutStandard>();

        if (j.contains("elementComposite")) {
            // Use static method from base class
            auto elementComposite = CompositeTypeDefinition::FromJson(j.at("elementComposite"));
            return std::make_shared<ShaderArrayDefinition>(elementComposite, arrayCount, standard);
        }
        else {
            BaseType elementType = j.at("elementType").get<BaseType>();
            return std::make_shared<ShaderArrayDefinition>(elementType, arrayCount, standard);
        }
    }

    // ============================================================================
    // SHADER ARRAY INSTANCE - IMPLEMENTATION
    // ============================================================================

    ShaderArrayInstance::ShaderArrayInstance(std::shared_ptr<const ShaderArrayDefinition> def)
        : definition(def)
        , elements(def->GetArrayCount())
        , buffer(def->GetSize(), 0) {

        if (!definition) {
            throw std::invalid_argument("Definition cannot be null");
        }

        InitializeElementDefaults();
    }

    ShaderArrayInstance::ShaderArrayInstance(const ShaderArrayInstance& other)
        : definition(other.definition)
        , elements(other.elements)
        , buffer(other.buffer) {
    }

    ShaderArrayInstance& ShaderArrayInstance::operator=(const ShaderArrayInstance& other) {
        if (this != &other) {
            definition = other.definition;
            elements = other.elements;
            buffer = other.buffer;
        }
        return *this;
    }

    void ShaderArrayInstance::ValidateIndex(uint32_t index) const {
        if (index >= definition->GetArrayCount()) {
            throw std::out_of_range("Array index " + std::to_string(index) +
                " out of bounds (size: " + std::to_string(definition->GetArrayCount()) + ")");
        }
    }

    void ShaderArrayInstance::InitializeElementDefaults() {
        if (definition->IsCompositeElement()) {
            // For composite types - create instances
            for (uint32_t i = 0; i < definition->GetArrayCount(); ++i) {
                auto instance = definition->GetElementComposite()->CreateInstance();
                elements[i] = ConvertCompositeToBufferValue(instance);
            }
        }
        else {
            // For base types - read from zeroed buffer
            for (uint32_t i = 0; i < definition->GetArrayCount(); ++i) {
                uint32_t offset = i * definition->GetElementStride();
                BufferValue defaultValue = ReadBaseTypeFromBuffer(
                    definition->GetElementBaseType(),
                    buffer.data() + offset);
                elements[i] = defaultValue;
            }
        }
    }

    void ShaderArrayInstance::WriteElementToBuffer(uint32_t index, const BufferValue& value) {
        uint32_t offset = index * definition->GetElementStride();
        if (!WriteBaseTypeToFixedBuffer(definition->GetElementBaseType(),
            buffer.data() + offset, value)) {
            throw std::invalid_argument("Failed to write element at index " + std::to_string(index));
        }
    }

    void ShaderArrayInstance::ReadElementFromBuffer(uint32_t index) {
        uint32_t offset = index * definition->GetElementStride();

        if (definition->IsCompositeElement()) {
            auto instance = definition->GetElementComposite()->CreateInstance();
            instance->ReadFromBuffer(buffer.data() + offset);
            elements[index] = ConvertCompositeToBufferValue(instance);
        }
        else {
            BufferValue value = ReadBaseTypeFromBuffer(
                definition->GetElementBaseType(),
                buffer.data() + offset);
            elements[index] = value;
        }
    }

    BufferValue ShaderArrayInstance::ConvertCompositeToBufferValue(
        std::shared_ptr<CompositeTypeInstance> instance) const {

        if (!instance) {
            throw std::invalid_argument("Instance cannot be null");
        }

        if (instance->IsStruct()) {
            return std::static_pointer_cast<ShaderStructInstance>(instance);
        }
        else if (instance->IsArray()) {
            return std::static_pointer_cast<ShaderArrayInstance>(instance);
        }

        throw std::invalid_argument("Unknown composite type");
    }

    bool ShaderArrayInstance::WriteToBuffer(void* dst) const {
        if (!dst) return false;
        std::memcpy(dst, buffer.data(), buffer.size());
        return true;
    }

    bool ShaderArrayInstance::ReadFromBuffer(const void* src) {
        if (!src) return false;

        std::memcpy(buffer.data(), src, buffer.size());

        // Read all elements from buffer
        for (uint32_t i = 0; i < definition->GetArrayCount(); ++i) {
            ReadElementFromBuffer(i);
        }

        return true;
    }

    json ShaderArrayInstance::ToJson() const {
        json result;
        result["type"] = "array";
        result["typeDef"] = definition->ToJson();  // Use unified ToJson

        json elementsJson = json::array();
        for (uint32_t i = 0; i < definition->GetArrayCount(); ++i) {
            if (definition->IsCompositeElement()) {
                auto composite = GetCompositeElement(i);
                elementsJson.push_back(composite->ToJson());
            }
            else {
                BufferValue value = GetElement(i);
                const auto& serInfo = GetSerializationInfo(definition->GetElementBaseType());
                if (!serInfo.SupportsJson()) {
                    throw std::runtime_error("Element type does not support JSON serialization");
                }
                elementsJson.push_back(serInfo.toJson(value));
            }
        }
        result["elements"] = elementsJson;

        return result;
    }

    bool ShaderArrayInstance::FromJson(const json& j) {
        if (!j.contains("elements")) return false;

        const json& elementsJson = j["elements"];

        if (elementsJson.size() != definition->GetArrayCount()) {
            return false;
        }

        for (uint32_t i = 0; i < definition->GetArrayCount(); ++i) {
            if (definition->IsCompositeElement()) {
                auto composite = definition->GetElementComposite()->CreateInstance();
                if (!composite->FromJson(elementsJson[i])) {
                    return false;
                }
                SetCompositeElement(i, composite);
            }
            else {
                const auto& serInfo = GetSerializationInfo(definition->GetElementBaseType());
                if (!serInfo.SupportsJson()) {
                    return false;
                }
                BufferValue value = serInfo.fromJson(elementsJson[i]);
                SetElement(i, value);
            }
        }

        return true;
    }

    void ShaderArrayInstance::SetElement(uint32_t index, const BufferValue& value) {
        ValidateIndex(index);

        if (definition->IsCompositeElement()) {
            throw std::invalid_argument("Use SetCompositeElement for composite types");
        }

        // Check type match
        BaseType valueType = GetBaseTypeFromVariant(value);
        if (definition->GetElementBaseType() != valueType) {
            throw std::invalid_argument("Type mismatch: expected " +
                std::string(BaseTypeToString(definition->GetElementBaseType())) +
                ", got " + std::string(BaseTypeToString(valueType)));
        }

        elements[index] = value;
        WriteElementToBuffer(index, value);
    }

    void ShaderArrayInstance::SetCompositeElement(uint32_t index,
        std::shared_ptr<CompositeTypeInstance> value) {
        ValidateIndex(index);

        if (!definition->IsCompositeElement()) {
            throw std::invalid_argument("Array element type is not composite");
        }
        if (!value) {
            throw std::invalid_argument("Composite value cannot be null");
        }
        if (value->GetDefinition()->GetTypeName() !=
            definition->GetElementComposite()->GetTypeName()) {
            throw std::invalid_argument("Composite type mismatch: expected " +
                definition->GetElementComposite()->GetTypeName() +
                ", got " + value->GetDefinition()->GetTypeName());
        }

        elements[index] = ConvertCompositeToBufferValue(value);

        uint32_t offset = index * definition->GetElementStride();
        const std::vector<uint8_t>& srcBuffer = value->GetRawBuffer();
        std::memcpy(buffer.data() + offset, srcBuffer.data(),
            std::min(srcBuffer.size(), static_cast<size_t>(definition->GetElementSize())));
    }

    BufferValue ShaderArrayInstance::GetElement(uint32_t index) const {
        ValidateIndex(index);
        return elements[index];
    }

    std::shared_ptr<CompositeTypeInstance> ShaderArrayInstance::GetCompositeElement(uint32_t index) const {
        ValidateIndex(index);

        const BufferValue& value = elements[index];

        if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
            return std::static_pointer_cast<CompositeTypeInstance>(*structPtr);
        }

        if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
            return std::static_pointer_cast<CompositeTypeInstance>(*arrayPtr);
        }

        throw std::invalid_argument("Element at index " + std::to_string(index) +
            " is not composite");
    }

    std::shared_ptr<const ShaderArrayDefinition> ShaderArrayInstance::GetArrayDefinition() const {
        return definition;
    }

} // namespace ShaderLib