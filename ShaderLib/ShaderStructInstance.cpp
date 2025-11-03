#include "pch.h"
#include "ShaderStructInstance.h"
#include "ShaderArrayInstance.h"
#include <cstring>
#include <algorithm>
#include "TypeSerializationTable.h"
#include "Serialization.h"
#include "ValueSerialization.h"

namespace ShaderLib {

    // ============================================================================
    // CONSTRUCTORS & ASSIGNMENT
    // ============================================================================

    ShaderStructInstance::ShaderStructInstance(std::shared_ptr<const ShaderStructDefinition> def)
        : definition(def)
        , buffer(def->GetSize(), 0)
        , bufferDirty(false) {

        if (!definition) {
            throw std::invalid_argument("Definition cannot be null");
        }
        if (!definition->IsFinalized()) {
            throw std::logic_error("Cannot create instance from non-finalized definition");
        }

        InitializeFieldDefaults();
    }

    ShaderStructInstance::ShaderStructInstance(const ShaderStructInstance& other)
        : definition(other.definition)
        , fieldValues(other.fieldValues)
        , buffer(other.buffer)
        , bufferDirty(other.bufferDirty) {
    }

    ShaderStructInstance::ShaderStructInstance(ShaderStructInstance&& other) noexcept
        : definition(std::move(other.definition))
        , fieldValues(std::move(other.fieldValues))
        , buffer(std::move(other.buffer))
        , bufferDirty(other.bufferDirty) {
        other.bufferDirty = false;
    }

    ShaderStructInstance& ShaderStructInstance::operator=(const ShaderStructInstance& other) {
        if (this != &other) {
            definition = other.definition;
            fieldValues = other.fieldValues;
            buffer = other.buffer;
            bufferDirty = other.bufferDirty;
        }
        return *this;
    }

    ShaderStructInstance& ShaderStructInstance::operator=(ShaderStructInstance&& other) noexcept {
        if (this != &other) {
            definition = std::move(other.definition);
            fieldValues = std::move(other.fieldValues);
            buffer = std::move(other.buffer);
            bufferDirty = other.bufferDirty;
            other.bufferDirty = false;
        }
        return *this;
    }

    ShaderStructInstance::ShaderStructInstance(
        std::shared_ptr<const ShaderStructDefinition> def,
        std::initializer_list<std::pair<const std::string, BufferValue>> fields)
        : ShaderStructInstance(def) {

        for (const auto& [fieldName, value] : fields) {
            at(fieldName) = value;
        }
    }

    // ============================================================================
    // ELEMENT ACCESS
    // ============================================================================

    BufferValue& ShaderStructInstance::at(const std::string& fieldName) {
        auto it = fieldValues.find(fieldName);
        if (it == fieldValues.end()) {
            throw std::out_of_range("Field '" + fieldName + "' not found");
        }
        bufferDirty = true;
        return it->second;
    }

    const BufferValue& ShaderStructInstance::at(const std::string& fieldName) const {
        auto it = fieldValues.find(fieldName);
        if (it == fieldValues.end()) {
            throw std::out_of_range("Field '" + fieldName + "' not found");
        }
        return it->second;
    }

    BufferValue& ShaderStructInstance::operator[](const std::string& fieldName) {
        const auto* field = definition->FindField(fieldName);
        if (!field) {
            throw std::out_of_range("Field '" + fieldName + "' not found in definition");
        }

        auto it = fieldValues.find(fieldName);
        if (it != fieldValues.end()) {
            bufferDirty = true;
            return it->second;
        }

        // Create default value for field
        if (field->IsComposite()) {
            auto instance = field->composite->CreateInstance();
            fieldValues[fieldName] = ConvertCompositeToBufferValue(instance);
        }
        else {
            BufferValue defaultValue = ReadBaseTypeFromBuffer(field->baseType,
                buffer.data() + field->offset);
            fieldValues[fieldName] = defaultValue;
        }

        bufferDirty = true;
        return fieldValues[fieldName];
    }

    // ============================================================================
    // COMPOSITE FIELD ACCESS
    // ============================================================================

    std::shared_ptr<CompositeTypeInstance> ShaderStructInstance::GetComposite(const std::string& fieldName) {
        auto it = fieldValues.find(fieldName);
        if (it == fieldValues.end()) {
            return nullptr;
        }

        const BufferValue& value = it->second;

        if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
            return std::static_pointer_cast<CompositeTypeInstance>(*structPtr);
        }

        if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
            return std::static_pointer_cast<CompositeTypeInstance>(*arrayPtr);
        }

        return nullptr;
    }

    std::shared_ptr<const CompositeTypeInstance> ShaderStructInstance::GetComposite(
        const std::string& fieldName) const {

        auto it = fieldValues.find(fieldName);
        if (it == fieldValues.end()) {
            return nullptr;
        }

        const BufferValue& value = it->second;

        if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
            return std::static_pointer_cast<const CompositeTypeInstance>(*structPtr);
        }

        if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
            return std::static_pointer_cast<const CompositeTypeInstance>(*arrayPtr);
        }

        return nullptr;
    }

    void ShaderStructInstance::SetComposite(const std::string& fieldName,
        std::shared_ptr<CompositeTypeInstance> value) {

        const auto* field = definition->FindField(fieldName);
        if (!field) {
            throw std::out_of_range("Field '" + fieldName + "' not found");
        }

        if (!field->IsComposite()) {
            throw std::invalid_argument("Field '" + fieldName + "' is not a composite type");
        }

        if (!value) {
            throw std::invalid_argument("Composite value cannot be null");
        }

        if (value->GetDefinition()->GetTypeName() != field->composite->GetTypeName()) {
            throw std::invalid_argument("Composite type mismatch for field '" + fieldName +
                "': expected " + field->composite->GetTypeName() +
                ", got " + value->GetDefinition()->GetTypeName());
        }

        fieldValues[fieldName] = ConvertCompositeToBufferValue(value);

        const std::vector<uint8_t>& srcBuffer = value->GetRawBuffer();
        std::memcpy(buffer.data() + field->offset, srcBuffer.data(),
            std::min(srcBuffer.size(), static_cast<size_t>(field->size)));
        
        bufferDirty = false;
    }

    // ============================================================================
    // MODIFIERS
    // ============================================================================

    void ShaderStructInstance::clear() {
        // Reset buffer to zeros
        std::fill(buffer.begin(), buffer.end(), 0);

        // Reinitialize all fields to defaults
        InitializeFieldDefaults();

        bufferDirty = false;
    }

    void ShaderStructInstance::swap(ShaderStructInstance& other) noexcept {
        using std::swap;
        swap(definition, other.definition);
        swap(fieldValues, other.fieldValues);
        swap(buffer, other.buffer);
    }

    // ============================================================================
    // CONVERSION TO/FROM MAP
    // ============================================================================

    std::unordered_map<std::string, BufferValue> ShaderStructInstance::ToMap() const {
        return fieldValues;
    }

    void ShaderStructInstance::FromMap(const std::unordered_map<std::string, BufferValue>& values) {
        for (const auto& [fieldName, value] : values) {
            const auto* field = definition->FindField(fieldName);
            if (!field) {
                throw std::invalid_argument("Field '" + fieldName + "' not found in definition");
            }

            if (field->IsComposite()) {
                auto composite = GetComposite(fieldName);
                if (!composite) {
                    throw std::invalid_argument("Failed to get composite field '" + fieldName + "'");
                }
                SetComposite(fieldName, composite);
            }
            else {
                // Type validation
                BaseType valueType = GetBaseTypeFromVariant(value);
                if (field->baseType != valueType) {
                    throw std::invalid_argument("Type mismatch for field '" + fieldName + "'");
                }

                fieldValues[fieldName] = value;
                WriteFieldToBuffer(*field, value);
            }
        }
    }

    // ============================================================================
    // FIELD INFORMATION
    // ============================================================================

    std::vector<std::string> ShaderStructInstance::GetFieldNames() const {
        std::vector<std::string> names;
        names.reserve(definition->GetFields().size());

        for (const auto& field : definition->GetFields()) {
            names.push_back(field.name);
        }

        return names;
    }

    bool ShaderStructInstance::IsFieldComposite(const std::string& fieldName) const {
        const auto* field = definition->FindField(fieldName);
        if (!field) {
            throw std::out_of_range("Field '" + fieldName + "' not found");
        }
        return field->IsComposite();
    }

    BaseType ShaderStructInstance::GetFieldBaseType(const std::string& fieldName) const {
        const auto* field = definition->FindField(fieldName);
        if (!field) {
            throw std::out_of_range("Field '" + fieldName + "' not found");
        }
        if (field->IsComposite()) {
            throw std::invalid_argument("Field '" + fieldName + "' is a composite type");
        }
        return field->baseType;
    }

    uint32_t ShaderStructInstance::GetFieldSize(const std::string& fieldName) const {
        const auto* field = definition->FindField(fieldName);
        if (!field) {
            throw std::out_of_range("Field '" + fieldName + "' not found");
        }
        return field->size;
    }

    uint32_t ShaderStructInstance::GetFieldOffset(const std::string& fieldName) const {
        const auto* field = definition->FindField(fieldName);
        if (!field) {
            throw std::out_of_range("Field '" + fieldName + "' not found");
        }
        return field->offset;
    }

    // ============================================================================
    // COMPARISON
    // ============================================================================

    bool ShaderStructInstance::operator==(const ShaderStructInstance& other) const {
        if (definition->GetTypeName() != other.definition->GetTypeName()) {
            return false;
        }

        if (fieldValues.size() != other.fieldValues.size()) {
            return false;
        }

        // Compare all fields
        for (const auto& field : definition->GetFields()) {
            auto it1 = fieldValues.find(field.name);
            auto it2 = other.fieldValues.find(field.name);

            if (it1 == fieldValues.end() || it2 == other.fieldValues.end()) {
                return false;
            }

            if (field.IsComposite()) {
                // Deep comparison for composite types
                auto comp1 = GetComposite(field.name);
                auto comp2 = other.GetComposite(field.name);

                if (!comp1 || !comp2) return false;

                if (comp1->GetRawBuffer() != comp2->GetRawBuffer()) {
                    return false;
                }
            }
            else {
                // Direct comparison for base types
                if (it1->second != it2->second) {
                    return false;
                }
            }
        }

        return true;
    }

    // ============================================================================
    // INTERNAL HELPER METHODS
    // ============================================================================

    void ShaderStructInstance::InitializeFieldDefaults() {
        for (const auto& field : definition->GetFields()) {
            if (field.IsComposite()) {
                auto instance = field.composite->CreateInstance();
                fieldValues[field.name] = ConvertCompositeToBufferValue(instance);
            }
            else {
                BufferValue defaultValue = ReadBaseTypeFromBuffer(field.baseType,
                    buffer.data() + field.offset);
                fieldValues[field.name] = defaultValue;
            }
        }
    }

    void ShaderStructInstance::WriteFieldToBuffer(const ShaderStructDefinition::Field& field,
        const BufferValue& value) {

        if (field.IsComposite()) {
            auto composite = GetComposite(field.name);
            if (composite) {
                const std::vector<uint8_t>& srcBuffer = composite->GetRawBuffer();
                std::memcpy(buffer.data() + field.offset, srcBuffer.data(),
                    std::min(srcBuffer.size(), static_cast<size_t>(field.size)));
            }
        }
        else {
            if (!WriteBaseTypeToFixedBuffer(field.baseType, buffer.data() + field.offset, value)) {
                throw std::invalid_argument("Failed to write field '" + field.name + "' to buffer");
            }
        }
    }

    void ShaderStructInstance::ReadFieldFromBuffer(const ShaderStructDefinition::Field& field) {
        if (field.IsComposite()) {
            auto instance = field.composite->CreateInstance();
            instance->ReadFromBuffer(buffer.data() + field.offset);
            fieldValues[field.name] = ConvertCompositeToBufferValue(instance);
        }
        else {
            BufferValue value = ReadBaseTypeFromBuffer(field.baseType,
                buffer.data() + field.offset);
            fieldValues[field.name] = value;
        }
    }

    BufferValue ShaderStructInstance::ConvertCompositeToBufferValue(
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

    void ShaderStructInstance::SyncBufferIfDirty() const {
        if (!bufferDirty) return;

        auto* self = const_cast<ShaderStructInstance*>(this);
        for (const auto& field : definition->GetFields()) {
            auto it = self->fieldValues.find(field.name);
            if (it != self->fieldValues.end()) {
                self->WriteFieldToBuffer(field, it->second);
            }
        }
        self->bufferDirty = false;
    }

    // ============================================================================
    // CompositeTypeInstance IMPLEMENTATION
    // ============================================================================

    bool ShaderStructInstance::WriteToBuffer(void* dst) const {
        if (!dst) return false;
        SyncBufferIfDirty();
        std::memcpy(dst, buffer.data(), buffer.size());
        return true;
    }

    bool ShaderStructInstance::ReadFromBuffer(const void* src) {
        if (!src) return false;

        std::memcpy(buffer.data(), src, buffer.size());

        for (const auto& field : definition->GetFields()) {
            ReadFieldFromBuffer(field);
        }

        bufferDirty = false;
        return true;
    }

    std::shared_ptr<CompositeTypeInstance> ShaderStructInstance::Clone() const {
        return std::make_shared<ShaderStructInstance>(*this);
    }

    json ShaderStructInstance::ToJson() const {
        json result;
        result["type"] = "struct";
        result["typeDef"] = definition->ToJson();

        json fieldsJson = json::object();
        for (const auto& field : definition->GetFields()) {
            if (field.IsComposite()) {
                auto composite = GetComposite(field.name);
                if (composite) {
                    fieldsJson[field.name] = composite->ToJson();
                }
            }
            else {
                auto it = fieldValues.find(field.name);
                if (it != fieldValues.end()) {
                    const auto& serInfo = GetSerializationInfo(field.baseType);
                    if (!serInfo.SupportsJson()) {
                        throw std::runtime_error("Field type does not support JSON serialization");
                    }
                    fieldsJson[field.name] = serInfo.toJson(it->second);
                }
            }
        }
        result["fields"] = fieldsJson;

        return result;
    }

    bool ShaderStructInstance::FromJson(const json& j) {
        if (!j.contains("fields")) return false;

        const json& fieldsJson = j["fields"];

        for (const auto& field : definition->GetFields()) {
            if (!fieldsJson.contains(field.name)) continue;

            if (field.IsComposite()) {
                auto composite = field.composite->CreateInstance();
                if (!composite->FromJson(fieldsJson[field.name])) {
                    return false;
                }
                SetComposite(field.name, composite);
            }
            else {
                const auto& serInfo = GetSerializationInfo(field.baseType);
                if (!serInfo.SupportsJson()) {
                    return false;
                }
                BufferValue value = serInfo.fromJson(fieldsJson[field.name]);
                fieldValues[field.name] = value;
                WriteFieldToBuffer(field, value);
            }
        }

        return true;
    }

} // namespace ShaderLib