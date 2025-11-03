#include "pch.h"
#include "ShaderArrayInstance.h"
#include "ShaderStructInstance.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include "TypeSerializationTable.h"
#include "Serialization.h"
#include "ValueSerialization.h"

namespace ShaderLib {

    // ============================================================================
    // CONSTRUCTORS & ASSIGNMENT
    // ============================================================================

    ShaderArrayInstance::ShaderArrayInstance(std::shared_ptr<const ShaderArrayDefinition> def)
        : definition(def)
        , elements(def->GetArrayCount())
        , buffer(def->GetSize(), 0)
        , bufferDirty(false) {

        if (!definition) {
            throw std::invalid_argument("Definition cannot be null");
        }

        InitializeElementDefaults();
    }

    ShaderArrayInstance::ShaderArrayInstance(const ShaderArrayInstance& other)
        : definition(other.definition)
        , elements(other.elements)
        , buffer(other.buffer)
        , bufferDirty(other.bufferDirty) {
    }

    ShaderArrayInstance::ShaderArrayInstance(ShaderArrayInstance&& other) noexcept
        : definition(std::move(other.definition))
        , elements(std::move(other.elements))
        , buffer(std::move(other.buffer))
        , bufferDirty(other.bufferDirty) {
        other.bufferDirty = false;
    }

    ShaderArrayInstance& ShaderArrayInstance::operator=(const ShaderArrayInstance& other) {
        if (this != &other) {
            definition = other.definition;
            elements = other.elements;
            buffer = other.buffer;
            bufferDirty = other.bufferDirty;
        }
        return *this;
    }

    ShaderArrayInstance& ShaderArrayInstance::operator=(ShaderArrayInstance&& other) noexcept {
        if (this != &other) {
            definition = std::move(other.definition);
            elements = std::move(other.elements);
            buffer = std::move(other.buffer);
            bufferDirty = other.bufferDirty;
            other.bufferDirty = false;
        }
        return *this;
    }

    ShaderArrayInstance::ShaderArrayInstance(
        std::shared_ptr<const ShaderArrayDefinition> def,
        const std::vector<BufferValue>& values)
        : ShaderArrayInstance(def) {
        FromVector(values);
    }

    ShaderArrayInstance::ShaderArrayInstance(
        std::shared_ptr<const ShaderArrayDefinition> def,
        std::initializer_list<BufferValue> values)
        : ShaderArrayInstance(def, std::vector<BufferValue>(values)) {
    }

    // ============================================================================
    // ELEMENT ACCESS
    // ============================================================================

    BufferValue& ShaderArrayInstance::at(size_type index) {
        if (index >= size()) {
            throw std::out_of_range("Array index " + std::to_string(index) +
                " out of bounds (size: " + std::to_string(size()) + ")");
        }
        bufferDirty = true;
        return elements[index];
    }

    const BufferValue& ShaderArrayInstance::at(size_type index) const {
        if (index >= size()) {
            throw std::out_of_range("Array index " + std::to_string(index) +
                " out of bounds (size: " + std::to_string(size()) + ")");
        }
        return elements[index];
    }

    BufferValue& ShaderArrayInstance::operator[](size_type index) {
        return elements[index];
        bufferDirty = true;
    }

    const BufferValue& ShaderArrayInstance::operator[](size_type index) const {
        return elements[index];
    }

    BufferValue& ShaderArrayInstance::front() {
        if (empty()) {
            throw std::out_of_range("Cannot access front of empty array");
        }
        bufferDirty = true;
        return elements.front();
    }

    const BufferValue& ShaderArrayInstance::front() const {
        if (empty()) {
            throw std::out_of_range("Cannot access front of empty array");
        }
        return elements.front();
    }

    BufferValue& ShaderArrayInstance::back() {
        if (empty()) {
            throw std::out_of_range("Cannot access back of empty array");
        }
        bufferDirty = true;
        return elements.back();
    }

    const BufferValue& ShaderArrayInstance::back() const {
        if (empty()) {
            throw std::out_of_range("Cannot access back of empty array");
        }
        return elements.back();
    }

    // ============================================================================
    // COMPOSITE ELEMENT ACCESS
    // ============================================================================

    std::shared_ptr<CompositeTypeInstance> ShaderArrayInstance::GetComposite(size_type index) {
        const BufferValue& value = at(index);

        if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
            return std::static_pointer_cast<CompositeTypeInstance>(*structPtr);
        }

        if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
            return std::static_pointer_cast<CompositeTypeInstance>(*arrayPtr);
        }

        return nullptr;
    }

    std::shared_ptr<const CompositeTypeInstance> ShaderArrayInstance::GetComposite(size_type index) const {
        const BufferValue& value = at(index);

        if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
            return std::static_pointer_cast<const CompositeTypeInstance>(*structPtr);
        }

        if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
            return std::static_pointer_cast<const CompositeTypeInstance>(*arrayPtr);
        }

        return nullptr;
    }

    void ShaderArrayInstance::SetComposite(size_type index,
        std::shared_ptr<CompositeTypeInstance> value) {

        if (index >= size()) {
            throw std::out_of_range("Array index out of bounds");
        }

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

        bufferDirty = false;
    }

    // ============================================================================
    // OPERATIONS
    // ============================================================================

    void ShaderArrayInstance::fill(const BufferValue& value) {
        // Validate type
        if (!definition->IsCompositeElement()) {
            BaseType valueType = GetBaseTypeFromVariant(value);
            if (definition->GetElementBaseType() != valueType) {
                throw std::invalid_argument("Type mismatch in fill operation");
            }
        }

        for (size_type i = 0; i < size(); ++i) {
            elements[i] = value;
            WriteElementToBuffer(i, value);
        }

        bufferDirty = false;
    }

    void ShaderArrayInstance::clear() {
        // Reset to default values
        std::fill(buffer.begin(), buffer.end(), 0);
        InitializeElementDefaults();
    }

    void ShaderArrayInstance::swap(ShaderArrayInstance& other) noexcept {
        using std::swap;
        swap(definition, other.definition);
        swap(elements, other.elements);
        swap(buffer, other.buffer);
    }

    // ============================================================================
    // CONVERSION TO/FROM VECTOR
    // ============================================================================

    std::vector<BufferValue> ShaderArrayInstance::ToVector() const {
        return elements;
    }

    void ShaderArrayInstance::FromVector(const std::vector<BufferValue>& values) {
        if (values.size() != size()) {
            throw std::invalid_argument("Vector size (" + std::to_string(values.size()) +
                ") must match array size (" + std::to_string(size()) + ")");
        }

        for (size_type i = 0; i < size(); ++i) {
            // Type validation happens in assignment
            if (!definition->IsCompositeElement()) {
                BaseType valueType = GetBaseTypeFromVariant(values[i]);
                if (definition->GetElementBaseType() != valueType) {
                    throw std::invalid_argument("Type mismatch at index " + std::to_string(i));
                }
            }

            elements[i] = values[i];
            WriteElementToBuffer(i, values[i]);
        }
        bufferDirty = false;
    }

    // ============================================================================
    // COMPARISON
    // ============================================================================

    bool ShaderArrayInstance::operator==(const ShaderArrayInstance& other) const {
        if (definition->GetTypeName() != other.definition->GetTypeName()) {
            return false;
        }

        if (size() != other.size()) {
            return false;
        }

        // For composite types, we need deep comparison
        if (definition->IsCompositeElement()) {
            for (size_type i = 0; i < size(); ++i) {
                auto comp1 = GetComposite(i);
                auto comp2 = other.GetComposite(i);

                if (!comp1 || !comp2) return false;

                // Compare buffers (deep comparison)
                if (comp1->GetRawBuffer() != comp2->GetRawBuffer()) {
                    return false;
                }
            }
            return true;
        }

        // For base types, direct comparison
        return elements == other.elements;
    }

    // ============================================================================
    // INTERNAL HELPER METHODS
    // ============================================================================

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

        if (definition->IsCompositeElement()) {
            auto composite = GetComposite(index);
            if (composite) {
                const std::vector<uint8_t>& srcBuffer = composite->GetRawBuffer();
                std::memcpy(buffer.data() + offset, srcBuffer.data(),
                    std::min(srcBuffer.size(), static_cast<size_t>(definition->GetElementSize())));
            }
        }
        else {
            if (!WriteBaseTypeToFixedBuffer(definition->GetElementBaseType(),
                buffer.data() + offset, value)) {
                throw std::invalid_argument("Failed to write element at index " + std::to_string(index));
            }
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

    void ShaderArrayInstance::SyncBufferIfDirty() const {
        if (!bufferDirty) return;

        auto* self = const_cast<ShaderArrayInstance*>(this);
        for (size_type i = 0; i < size(); ++i) {
            self->WriteElementToBuffer(i, elements[i]);
        }
        bufferDirty = false;
    }

    // ============================================================================
    // CompositeTypeInstance IMPLEMENTATION
    // ============================================================================

    bool ShaderArrayInstance::WriteToBuffer(void* dst) const {
        if (!dst) return false;
        SyncBufferIfDirty();
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

    std::shared_ptr<CompositeTypeInstance> ShaderArrayInstance::Clone() const {
        return std::make_shared<ShaderArrayInstance>(*this);
    }

    json ShaderArrayInstance::ToJson() const {
        json result;
        result["type"] = "array";
        result["typeDef"] = definition->ToJson();

        json elementsJson = json::array();
        for (uint32_t i = 0; i < definition->GetArrayCount(); ++i) {
            if (definition->IsCompositeElement()) {
                auto composite = GetComposite(i);
                if (composite) {
                    elementsJson.push_back(composite->ToJson());
                }
            }
            else {
                const BufferValue& value = at(i);
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
                SetComposite(i, composite);
            }
            else {
                const auto& serInfo = GetSerializationInfo(definition->GetElementBaseType());
                if (!serInfo.SupportsJson()) {
                    return false;
                }
                BufferValue value = serInfo.fromJson(elementsJson[i]);
                elements[i] = value;
                WriteElementToBuffer(i, value);
            }
        }

        return true;
    }

} // namespace ShaderLib