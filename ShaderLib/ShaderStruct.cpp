#include "pch.h"
#include "ShaderStruct.h"
#include "ShaderArray.h"
#include <sstream>
#include <cstring>
#include <algorithm>
#include "TypeSerializationTable.h"
#include "Serialization.h"
#include "ValueSerialization.h"

namespace ShaderLib {

    // ============================================================================
    // FIELD IMPLEMENTATION
    // ============================================================================

    ShaderStructDefinition::Field::Field(const std::string& n, BaseType t, uint32_t off, uint32_t sz)
        : name(n), baseType(t), offset(off), size(sz), composite(nullptr) {
    }

    ShaderStructDefinition::Field::Field(const std::string& n,
        std::shared_ptr<const CompositeTypeDefinition> comp,
        uint32_t off)
        : name(n), baseType(BaseType::Unknown), offset(off),
        size(comp->GetSize()), composite(comp) {
    }

    // ============================================================================
    // SHADER STRUCT DEFINITION - IMPLEMENTATION
    // ============================================================================

    ShaderStructDefinition::ShaderStructDefinition(const std::string& name, LayoutStandard standard)
        : typeName(name), size(0), alignment(0), layoutStandard(standard), finalized(false) {
    }

    const ShaderStructDefinition::Field* ShaderStructDefinition::FindField(const std::string& name) const {
        auto it = std::find_if(fields.begin(), fields.end(),
            [&name](const Field& f) { return f.name == name; });
        return (it != fields.end()) ? &(*it) : nullptr;
    }

    ShaderStructDefinition& ShaderStructDefinition::AddField(const std::string& name, BaseType type) {
        if (finalized) {
            throw std::logic_error("Cannot add fields to finalized struct");
        }
        if (type == BaseType::Unknown) {
            throw std::invalid_argument("BaseType::Unknown is not valid");
        }
        if (type == BaseType::Struct || type == BaseType::Array) {
            throw std::invalid_argument("Use AddCompositeField for structs/arrays");
        }

        const BaseTypeInfo& info = GetBaseTypeInfo(type);
        uint32_t fieldAlignment = info.GetAlignment(layoutStandard);

        size = AlignTo(size, fieldAlignment);
        fields.emplace_back(name, type, size, info.size);
        size += info.size;
        alignment = std::max(alignment, fieldAlignment);

        return *this;
    }

    ShaderStructDefinition& ShaderStructDefinition::AddCompositeField(
        const std::string& name,
        std::shared_ptr<const CompositeTypeDefinition> composite) {

        if (finalized) {
            throw std::logic_error("Cannot add fields to finalized struct");
        }
        if (!composite) {
            throw std::invalid_argument("Composite type cannot be null");
        }

        uint32_t fieldAlignment = composite->GetAlignment();
        size = AlignTo(size, fieldAlignment);
        fields.emplace_back(name, composite, size);
        size += composite->GetSize();
        alignment = std::max(alignment, fieldAlignment);

        return *this;
    }

    void ShaderStructDefinition::Finalize() {
        if (!finalized) {
            if (alignment > 0) {
                size = AlignTo(size, alignment);
            }
            finalized = true;
        }
    }

    json ShaderStructDefinition::ToJson() const {
        json result;
        result["compositeType"] = "struct";  // Type discriminator
        result["name"] = typeName;
        result["layoutStandard"] = layoutStandard;

        json fieldsArray = json::array();
        for (const auto& field : fields) {
            json fieldJson;
            fieldJson["name"] = field.name;

            if (field.IsComposite()) {
                fieldJson["composite"] = field.composite->ToJson();
            }
            else {
                fieldJson["baseType"] = field.baseType;
            }

            fieldsArray.push_back(fieldJson);
        }
        result["fields"] = fieldsArray;

        return result;
    }

    std::string ShaderStructDefinition::GenerateGLSL() const {
        std::stringstream ss;
        ss << "struct " << typeName << " {\n";

        for (const auto& field : fields) {
            ss << "    ";

            if (field.IsComposite()) {
                ss << field.composite->GetTypeName();
            }
            else {
                ss << BaseTypeToString(field.baseType);
            }

            ss << " " << field.name << ";\n";
        }

        ss << "};\n";
        return ss.str();
    }

    std::shared_ptr<CompositeTypeInstance> ShaderStructDefinition::CreateInstance() const {
        if (!finalized) {
            throw std::logic_error("Cannot create instance from non-finalized definition");
        }
        return std::make_shared<ShaderStructInstance>(shared_from_this());
    }

    std::shared_ptr<ShaderStructDefinition> ShaderStructDefinition::FromJson(const json& j) {
        std::string name = j.at("name").get<std::string>();
        LayoutStandard standard = j.at("layoutStandard").get<LayoutStandard>();

        auto structDef = std::make_shared<ShaderStructDefinition>(name, standard);

        const json& fields = j.at("fields");
        for (const auto& fieldJson : fields) {
            std::string fieldName = fieldJson.at("name").get<std::string>();

            if (fieldJson.contains("composite")) {
                // Use static method from base class
                auto composite = CompositeTypeDefinition::FromJson(fieldJson.at("composite"));
                structDef->AddCompositeField(fieldName, composite);
            }
            else {
                BaseType baseType = fieldJson.at("baseType").get<BaseType>();
                structDef->AddField(fieldName, baseType);
            }
        }

        structDef->Finalize();
        return structDef;
    }

    // ============================================================================
    // SHADER STRUCT INSTANCE - IMPLEMENTATION
    // ============================================================================

    ShaderStructInstance::ShaderStructInstance(std::shared_ptr<const ShaderStructDefinition> def)
        : definition(def)
        , buffer(def->GetSize(), 0) {

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
        , buffer(other.buffer) {
    }

    ShaderStructInstance& ShaderStructInstance::operator=(const ShaderStructInstance& other) {
        if (this != &other) {
            definition = other.definition;
            fieldValues = other.fieldValues;
            buffer = other.buffer;
        }
        return *this;
    }

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
        if (!WriteBaseTypeToFixedBuffer(field.baseType, buffer.data() + field.offset, value)) {
            throw std::invalid_argument("Failed to write field '" + field.name + "' to buffer");
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

    bool ShaderStructInstance::WriteToBuffer(void* dst) const {
        if (!dst) return false;
        std::memcpy(dst, buffer.data(), buffer.size());
        return true;
    }

    bool ShaderStructInstance::ReadFromBuffer(const void* src) {
        if (!src) return false;

        std::memcpy(buffer.data(), src, buffer.size());

        for (const auto& field : definition->GetFields()) {
            ReadFieldFromBuffer(field);
        }

        return true;
    }

    json ShaderStructInstance::ToJson() const {
        json result;
        result["type"] = "struct";
        result["typeDef"] = definition->ToJson();  // Use unified ToJson

        json fieldsJson = json::object();
        for (const auto& field : definition->GetFields()) {
            if (field.IsComposite()) {
                auto composite = GetCompositeField(field.name);
                fieldsJson[field.name] = composite->ToJson();
            }
            else {
                BufferValue value = GetField(field.name);
                const auto& serInfo = GetSerializationInfo(field.baseType);
                if (!serInfo.SupportsJson()) {
                    throw std::runtime_error("Field type does not support JSON serialization");
                }
                fieldsJson[field.name] = serInfo.toJson(value);
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
                SetCompositeField(field.name, composite);
            }
            else {
                const auto& serInfo = GetSerializationInfo(field.baseType);
                if (!serInfo.SupportsJson()) {
                    return false;
                }
                BufferValue value = serInfo.fromJson(fieldsJson[field.name]);
                SetField(field.name, value);
            }
        }

        return true;
    }

    void ShaderStructInstance::SetField(const std::string& fieldName, const BufferValue& value) {
        const auto* field = definition->FindField(fieldName);
        if (!field) {
            throw std::out_of_range("Field '" + fieldName + "' not found");
        }
        if (field->IsComposite()) {
            throw std::invalid_argument("Use SetCompositeField for composite types");
        }

        BaseType valueType = GetBaseTypeFromVariant(value);
        if (field->baseType != valueType) {
            throw std::invalid_argument("Type mismatch for field '" + fieldName + "'");
        }

        fieldValues[fieldName] = value;
        WriteFieldToBuffer(*field, value);
    }

    void ShaderStructInstance::SetCompositeField(const std::string& fieldName,
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
            throw std::invalid_argument("Composite type mismatch for field '" + fieldName + "'");
        }

        fieldValues[fieldName] = ConvertCompositeToBufferValue(value);

        const std::vector<uint8_t>& srcBuffer = value->GetRawBuffer();
        std::memcpy(buffer.data() + field->offset, srcBuffer.data(),
            std::min(srcBuffer.size(), static_cast<size_t>(field->size)));
    }

    BufferValue ShaderStructInstance::GetField(const std::string& fieldName) const {
        auto it = fieldValues.find(fieldName);
        if (it == fieldValues.end()) {
            throw std::out_of_range("Field '" + fieldName + "' not set");
        }

        return it->second;
    }

    std::shared_ptr<CompositeTypeInstance> ShaderStructInstance::GetCompositeField(
        const std::string& fieldName) const {

        auto it = fieldValues.find(fieldName);
        if (it == fieldValues.end()) {
            throw std::out_of_range("Field '" + fieldName + "' not set");
        }

        const BufferValue& value = it->second;

        if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
            return std::static_pointer_cast<CompositeTypeInstance>(*structPtr);
        }

        if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
            return std::static_pointer_cast<CompositeTypeInstance>(*arrayPtr);
        }

        throw std::invalid_argument("Field '" + fieldName + "' is not a composite type");
    }

    bool ShaderStructInstance::HasField(const std::string& fieldName) const {
        return fieldValues.find(fieldName) != fieldValues.end();
    }

} // namespace ShaderLib