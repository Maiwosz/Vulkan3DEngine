#include "pch.h"
//#include "BufferObjectInstance.h"
//#include "ShaderStructInstance.h"
//#include "ShaderArrayInstance.h"
//#include "ValueSerialization.h"
//#include <cstring>
//#include <algorithm>
//
//namespace ShaderLib {
//
//    // ============================================================================
//    // CONSTRUCTORS & ASSIGNMENT
//    // ============================================================================
//
//    BufferObjectInstance::BufferObjectInstance(
//        std::shared_ptr<const BufferObjectDefinition> def)
//        : definition(def)
//        , buffer(def->GetSize(), 0)
//        , bufferDirty(false) {
//
//        if (!definition) {
//            throw std::invalid_argument("Definition cannot be null");
//        }
//
//        InitializeDefaults();
//    }
//
//    BufferObjectInstance::BufferObjectInstance(const BufferObjectInstance& other)
//        : definition(other.definition)
//        , values(other.values)
//        , buffer(other.buffer)
//        , bufferDirty(other.bufferDirty) {
//    }
//
//    BufferObjectInstance::BufferObjectInstance(BufferObjectInstance&& other) noexcept
//        : definition(std::move(other.definition))
//        , values(std::move(other.values))
//        , buffer(std::move(other.buffer))
//        , bufferDirty(other.bufferDirty) {
//        other.bufferDirty = false;
//    }
//
//    BufferObjectInstance& BufferObjectInstance::operator=(const BufferObjectInstance& other) {
//        if (this != &other) {
//            definition = other.definition;
//            values = other.values;
//            buffer = other.buffer;
//            bufferDirty = other.bufferDirty;
//        }
//        return *this;
//    }
//
//    BufferObjectInstance& BufferObjectInstance::operator=(BufferObjectInstance&& other) noexcept {
//        if (this != &other) {
//            definition = std::move(other.definition);
//            values = std::move(other.values);
//            buffer = std::move(other.buffer);
//            bufferDirty = other.bufferDirty;
//            other.bufferDirty = false;
//        }
//        return *this;
//    }
//
//    // ============================================================================
//    // ELEMENT ACCESS
//    // ============================================================================
//
//    BufferValue& BufferObjectInstance::at(const std::string& fieldName) {
//        auto it = values.find(fieldName);
//        if (it == values.end()) {
//            throw std::out_of_range("Field '" + fieldName + "' not found");
//        }
//        bufferDirty = true;
//        return it->second;
//    }
//
//    const BufferValue& BufferObjectInstance::at(const std::string& fieldName) const {
//        auto it = values.find(fieldName);
//        if (it == values.end()) {
//            throw std::out_of_range("Field '" + fieldName + "' not found");
//        }
//        return it->second;
//    }
//
//    BufferValue& BufferObjectInstance::operator[](const std::string& fieldName) {
//        bufferDirty = true;
//        return values[fieldName];
//    }
//
//    std::shared_ptr<CompositeTypeInstance> BufferObjectInstance::GetComposite(
//        const std::string& fieldName) {
//
//        const BufferValue& value = at(fieldName);
//
//        if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
//            return std::static_pointer_cast<CompositeTypeInstance>(*structPtr);
//        }
//
//        if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
//            return std::static_pointer_cast<CompositeTypeInstance>(*arrayPtr);
//        }
//
//        return nullptr;
//    }
//
//    std::shared_ptr<const CompositeTypeInstance> BufferObjectInstance::GetComposite(
//        const std::string& fieldName) const {
//
//        const BufferValue& value = at(fieldName);
//
//        if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
//            return std::static_pointer_cast<const CompositeTypeInstance>(*structPtr);
//        }
//
//        if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
//            return std::static_pointer_cast<const CompositeTypeInstance>(*arrayPtr);
//        }
//
//        return nullptr;
//    }
//
//    std::shared_ptr<ShaderStructInstance> BufferObjectInstance::GetStruct(
//        const std::string& fieldName) {
//
//        const BufferValue& value = at(fieldName);
//
//        if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
//            return *structPtr;
//        }
//
//        return nullptr;
//    }
//
//    std::shared_ptr<const ShaderStructInstance> BufferObjectInstance::GetStruct(
//        const std::string& fieldName) const {
//
//        const BufferValue& value = at(fieldName);
//
//        if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
//            return *structPtr;
//        }
//
//        return nullptr;
//    }
//
//    std::shared_ptr<ShaderArrayInstance> BufferObjectInstance::GetArray(
//        const std::string& fieldName) {
//
//        const BufferValue& value = at(fieldName);
//
//        if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
//            return *arrayPtr;
//        }
//
//        return nullptr;
//    }
//
//    std::shared_ptr<const ShaderArrayInstance> BufferObjectInstance::GetArray(
//        const std::string& fieldName) const {
//
//        const BufferValue& value = at(fieldName);
//
//        if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
//            return *arrayPtr;
//        }
//
//        return nullptr;
//    }
//
//    // ============================================================================
//    // SETTERS
//    // ============================================================================
//
//    void BufferObjectInstance::Set(const std::string& fieldName, const BufferValue& value) {
//        const BufferFieldDefinition* field = definition->FindField(fieldName);
//        if (!field) {
//            throw std::out_of_range("Field '" + fieldName + "' not found");
//        }
//
//        // Type validation
//        if (field->IsComposite()) {
//            // Check if value is composite
//            bool isComposite = std::holds_alternative<std::shared_ptr<ShaderStructInstance>>(value) ||
//                std::holds_alternative<std::shared_ptr<ShaderArrayInstance>>(value);
//
//            if (!isComposite) {
//                throw std::invalid_argument("Field '" + fieldName + "' requires composite type");
//            }
//
//            // Validate type matches
//            std::shared_ptr<const CompositeTypeDefinition> valueDef = nullptr;
//
//            if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
//                valueDef = (*structPtr)->GetDefinition();
//            }
//            else if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
//                valueDef = (*arrayPtr)->GetDefinition();
//            }
//
//            if (valueDef && valueDef->GetTypeName() != field->composite->GetTypeName()) {
//                throw std::invalid_argument("Type mismatch for field '" + fieldName + "'");
//            }
//        }
//        else {
//            // Base type validation
//            BaseType valueType = GetBaseTypeFromVariant(value);
//            if (field->baseType != valueType) {
//                throw std::invalid_argument("Type mismatch for field '" + fieldName + "'");
//            }
//        }
//
//        values[fieldName] = value;
//        WriteFieldToBuffer(fieldName, value);
//        bufferDirty = false;
//    }
//
//    void BufferObjectInstance::SetComposite(const std::string& fieldName,
//        std::shared_ptr<CompositeTypeInstance> value) {
//
//        const BufferFieldDefinition* field = definition->FindField(fieldName);
//        if (!field) {
//            throw std::out_of_range("Field '" + fieldName + "' not found");
//        }
//
//        if (!field->IsComposite()) {
//            throw std::invalid_argument("Field '" + fieldName + "' is not composite");
//        }
//
//        if (!value) {
//            throw std::invalid_argument("Composite value cannot be null");
//        }
//
//        if (value->GetDefinition()->GetTypeName() != field->composite->GetTypeName()) {
//            throw std::invalid_argument("Type mismatch for field '" + fieldName + "'");
//        }
//
//        // Convert to appropriate BufferValue type
//        if (value->IsStruct()) {
//            values[fieldName] = std::static_pointer_cast<ShaderStructInstance>(value);
//        }
//        else if (value->IsArray()) {
//            values[fieldName] = std::static_pointer_cast<ShaderArrayInstance>(value);
//        }
//        else {
//            throw std::invalid_argument("Unknown composite type");
//        }
//
//        WriteFieldToBuffer(fieldName, values[fieldName]);
//        bufferDirty = false;
//    }
//
//    // ============================================================================
//    // OPERATIONS
//    // ============================================================================
//
//    void BufferObjectInstance::clear() {
//        std::fill(buffer.begin(), buffer.end(), 0);
//        InitializeDefaults();
//    }
//
//    void BufferObjectInstance::swap(BufferObjectInstance& other) noexcept {
//        using std::swap;
//        swap(definition, other.definition);
//        swap(values, other.values);
//        swap(buffer, other.buffer);
//        swap(bufferDirty, other.bufferDirty);
//    }
//
//    // ============================================================================
//    // BUFFER MANAGEMENT
//    // ============================================================================
//
//    bool BufferObjectInstance::WriteToBuffer(void* dst) const {
//        if (!dst) return false;
//        SyncBufferIfDirty();
//        std::memcpy(dst, buffer.data(), buffer.size());
//        return true;
//    }
//
//    bool BufferObjectInstance::ReadFromBuffer(const void* src) {
//        if (!src) return false;
//
//        std::memcpy(buffer.data(), src, buffer.size());
//
//        // Read all fields from buffer
//        for (const auto& field : *definition) {
//            ReadFieldFromBuffer(field.name);
//        }
//
//        bufferDirty = false;
//        return true;
//    }
//
//    // ============================================================================
//    // TYPE CHECKING
//    // ============================================================================
//
//    bool BufferObjectInstance::HasField(const std::string& fieldName) const {
//        return definition->FindField(fieldName) != nullptr;
//    }
//
//    bool BufferObjectInstance::IsBaseField(const std::string& fieldName) const {
//        const BufferFieldDefinition* field = definition->FindField(fieldName);
//        return field && field->IsBase();
//    }
//
//    bool BufferObjectInstance::IsCompositeField(const std::string& fieldName) const {
//        const BufferFieldDefinition* field = definition->FindField(fieldName);
//        return field && field->IsComposite();
//    }
//
//    bool BufferObjectInstance::IsStructField(const std::string& fieldName) const {
//        const BufferFieldDefinition* field = definition->FindField(fieldName);
//        return field && field->IsStruct();
//    }
//
//    bool BufferObjectInstance::IsArrayField(const std::string& fieldName) const {
//        const BufferFieldDefinition* field = definition->FindField(fieldName);
//        return field && field->IsArray();
//    }
//
//    // ============================================================================
//    // CLONING
//    // ============================================================================
//
//    std::shared_ptr<BufferObjectInstance> BufferObjectInstance::Clone() const {
//        return std::make_shared<BufferObjectInstance>(*this);
//    }
//
//    // ============================================================================
//    // COMPARISON
//    // ============================================================================
//
//    bool BufferObjectInstance::operator==(const BufferObjectInstance& other) const {
//        if (definition->GetName() != other.definition->GetName()) {
//            return false;
//        }
//
//        if (values.size() != other.values.size()) {
//            return false;
//        }
//
//        // Deep comparison
//        for (const auto& [fieldName, value] : values) {
//            auto it = other.values.find(fieldName);
//            if (it == other.values.end()) {
//                return false;
//            }
//
//            const BufferFieldDefinition* field = definition->FindField(fieldName);
//            if (!field) return false;
//
//            if (field->IsComposite()) {
//                auto comp1 = GetComposite(fieldName);
//                auto comp2 = other.GetComposite(fieldName);
//
//                if (!comp1 || !comp2) return false;
//
//                if (comp1->GetRawBuffer() != comp2->GetRawBuffer()) {
//                    return false;
//                }
//            }
//            else {
//                if (value != it->second) {
//                    return false;
//                }
//            }
//        }
//
//        return true;
//    }
//
//    // ============================================================================
//    // VALIDATION
//    // ============================================================================
//
//    bool BufferObjectInstance::Validate() const {
//        if (!definition) return false;
//        if (buffer.size() != definition->GetSize()) return false;
//
//        // Validate all fields exist
//        for (const auto& field : *definition) {
//            if (values.find(field.name) == values.end()) {
//                return false;
//            }
//        }
//
//        return true;
//    }
//
//    // ============================================================================
//    // INTERNAL HELPER METHODS
//    // ============================================================================
//
//    void BufferObjectInstance::InitializeDefaults() {
//        values.clear();
//
//        for (const auto& field : *definition) {
//            if (field.IsComposite()) {
//                // Create composite instance
//                auto instance = field.composite->CreateInstance();
//
//                if (field.IsStruct()) {
//                    values[field.name] = std::static_pointer_cast<ShaderStructInstance>(instance);
//                }
//                else if (field.IsArray()) {
//                    values[field.name] = std::static_pointer_cast<ShaderArrayInstance>(instance);
//                }
//            }
//            else {
//                // Read default value from zeroed buffer
//                uint8_t* fieldPtr = buffer.data() + field.offset;
//                BufferValue defaultValue = ReadBaseTypeFromBuffer(field.baseType, fieldPtr);
//                values[field.name] = defaultValue;
//            }
//        }
//    }
//
//    void BufferObjectInstance::WriteFieldToBuffer(const std::string& fieldName,
//        const BufferValue& value) {
//
//        const BufferFieldDefinition* field = definition->FindField(fieldName);
//        if (!field) return;
//
//        uint8_t* dst = buffer.data() + field->offset;
//
//        if (field->IsComposite()) {
//            auto composite = GetComposite(fieldName);
//            if (composite) {
//                const std::vector<uint8_t>& srcBuffer = composite->GetRawBuffer();
//                std::memcpy(dst, srcBuffer.data(),
//                    std::min(srcBuffer.size(), static_cast<size_t>(field->size)));
//            }
//        }
//        else {
//            WriteBaseTypeToFixedBuffer(field->baseType, dst, value);
//        }
//    }
//
//    void BufferObjectInstance::ReadFieldFromBuffer(const std::string& fieldName) {
//        const BufferFieldDefinition* field = definition->FindField(fieldName);
//        if (!field) return;
//
//        uint8_t* src = buffer.data() + field->offset;
//
//        if (field->IsComposite()) {
//            auto instance = field->composite->CreateInstance();
//            instance->ReadFromBuffer(src);
//
//            if (instance->IsStruct()) {
//                values[fieldName] = std::static_pointer_cast<ShaderStructInstance>(instance);
//            }
//            else if (instance->IsArray()) {
//                values[fieldName] = std::static_pointer_cast<ShaderArrayInstance>(instance);
//            }
//        }
//        else {
//            BufferValue value = ReadBaseTypeFromBuffer(field->baseType, src);
//            values[fieldName] = value;
//        }
//    }
//
//    void BufferObjectInstance::SyncBufferIfDirty() const {
//        if (!bufferDirty) return;
//
//        auto* self = const_cast<BufferObjectInstance*>(this);
//        for (const auto& [fieldName, value] : values) {
//            self->WriteFieldToBuffer(fieldName, value);
//        }
//        bufferDirty = false;
//    }
//
//} // namespace ShaderLib