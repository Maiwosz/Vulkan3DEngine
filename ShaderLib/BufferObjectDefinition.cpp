#include "pch.h"
//#include "BufferObjectDefinition.h"
//#include "BufferObjectInstance.h"
//#include "ShaderArrayDefinition.h"
//#include "ShaderStructDefinition.h"
//#include "TypeSerializationTable.h"
//#include <sstream>
//#include <algorithm>
//#include <set>
//
//namespace ShaderLib {
//
//    // ============================================================================
//    // BufferFieldDefinition Implementation
//    // ============================================================================
//
//    std::string BufferFieldDefinition::GetTypeName() const {
//        if (IsComposite()) {
//            return composite->GetTypeName();
//        }
//        return BaseTypeToString(baseType);
//    }
//
//    ShaderTypeCategory BufferFieldDefinition::GetCategory() const {
//        if (IsComposite()) {
//            return ShaderTypeCategory::Composite;
//        }
//        return (baseType != BaseType::Unknown)
//            ? ShaderTypeCategory::Base
//            : ShaderTypeCategory::Unknown;
//    }
//
//    std::string BufferFieldDefinition::GenerateGLSLDeclaration() const {
//        std::stringstream ss;
//
//        if (IsComposite() && composite->IsArray()) {
//            // Array: "elementType name[count]"
//            auto arrayDef = std::static_pointer_cast<const ShaderArrayDefinition>(composite);
//
//            if (arrayDef->IsCompositeElement()) {
//                ss << arrayDef->GetElementComposite()->GetTypeName();
//            }
//            else {
//                ss << BaseTypeToString(arrayDef->GetElementBaseType());
//            }
//            ss << " " << name << "[" << arrayDef->GetArrayCount() << "]";
//        }
//        else {
//            // Base type or struct: "typeName name"
//            ss << GetTypeName() << " " << name;
//        }
//
//        return ss.str();
//    }
//
//    json BufferFieldDefinition::ToJson() const {
//        json result;
//        result["name"] = name;
//        result["size"] = size;
//        result["offset"] = offset;
//        result["accessMode"] = static_cast<uint8_t>(accessMode);
//
//        if (IsComposite()) {
//            result["composite"] = composite->ToJson();
//        }
//        else {
//            result["baseType"] = static_cast<uint32_t>(baseType);
//        }
//
//        return result;
//    }
//
//    BufferFieldDefinition BufferFieldDefinition::FromJson(const json& j) {
//        BufferFieldDefinition field;
//        field.name = j.at("name").get<std::string>();
//        field.size = j.at("size").get<uint32_t>();
//        field.offset = j.at("offset").get<uint32_t>();
//        field.accessMode = static_cast<BufferAccessMode>(j.at("accessMode").get<uint8_t>());
//
//        if (j.contains("composite")) {
//            field.composite = CompositeTypeDefinition::FromJson(j.at("composite"));
//            field.baseType = field.composite->IsStruct() ? BaseType::Struct : BaseType::Array;
//        }
//        else {
//            field.baseType = static_cast<BaseType>(j.at("baseType").get<uint32_t>());
//            field.composite = nullptr;
//        }
//
//        return field;
//    }
//
//    // ============================================================================
//    // BufferObjectDefinition Implementation
//    // ============================================================================
//
//    BufferObjectDefinition::BufferObjectDefinition(
//        const std::string& bufferName,
//        BufferType type,
//        LayoutStandard standard,
//        BufferAccessMode accessMode,
//        const std::vector<BufferFieldDefinition>& bufferFields,
//        bool useInstance)
//        : name(bufferName)
//        , totalSize(0)
//        , bufferType(type)
//        , layoutStandard(standard)
//        , bufferAccessMode(accessMode)
//        , fields(bufferFields)
//        , useInstanceName(useInstance) {
//
//        // Compute total size with proper alignment
//        if (!fields.empty()) {
//            const auto& lastField = fields.back();
//            uint32_t rawSize = lastField.offset + lastField.size;
//
//            // Final alignment: 16 for UBO, 4 for SSBO
//            uint32_t finalAlignment = (bufferType == BufferType::Uniform) ? 16 : 4;
//            totalSize = AlignTo(rawSize, finalAlignment);
//        }
//
//        BuildFieldCache();
//    }
//
//    void BufferObjectDefinition::BuildFieldCache() const {
//        fieldIndexCache.clear();
//        for (size_t i = 0; i < fields.size(); ++i) {
//            fieldIndexCache[fields[i].name] = i;
//        }
//    }
//
//    const BufferFieldDefinition& BufferObjectDefinition::GetField(size_t index) const {
//        if (index >= fields.size()) {
//            throw std::out_of_range("Field index out of range");
//        }
//        return fields[index];
//    }
//
//    const BufferFieldDefinition* BufferObjectDefinition::FindField(const std::string& fieldName) const {
//        if (fieldIndexCache.empty()) {
//            BuildFieldCache();
//        }
//
//        auto it = fieldIndexCache.find(fieldName);
//        if (it != fieldIndexCache.end()) {
//            return &fields[it->second];
//        }
//        return nullptr;
//    }
//
//    std::string BufferObjectDefinition::GetAccessQualifier() const {
//        if (bufferType == BufferType::Uniform) {
//            return "";
//        }
//
//        switch (bufferAccessMode) {
//        case BufferAccessMode::ReadOnly: return "readonly";
//        case BufferAccessMode::WriteOnly: return "writeonly";
//        case BufferAccessMode::ReadWrite: return "";
//        default: return "";
//        }
//    }
//
//    std::string BufferObjectDefinition::GenerateGLSL(uint32_t setNumber, uint32_t binding) const {
//        std::stringstream ss;
//
//        // Collect unique composite type definitions
//        std::set<std::string> definitions;
//        for (const auto& field : fields) {
//            if (field.IsComposite()) {
//                std::string glsl = field.composite->GenerateGLSL();
//                if (!glsl.empty()) {
//                    definitions.insert(glsl);
//                }
//            }
//        }
//
//        // Output struct definitions
//        for (const auto& def : definitions) {
//            ss << def << "\n";
//        }
//
//        if (!definitions.empty()) {
//            ss << "\n";
//        }
//
//        // Generate buffer declaration
//        const char* layoutKeyword = (layoutStandard == LayoutStandard::Std140)
//            ? "std140" : "std430";
//        const char* bufferKeyword = (bufferType == BufferType::Uniform)
//            ? "uniform" : "buffer";
//
//        std::string accessQualifier = GetAccessQualifier();
//        if (!accessQualifier.empty()) {
//            accessQualifier += " ";
//        }
//
//        ss << "layout(" << layoutKeyword << ", set = " << setNumber
//            << ", binding = " << binding << ") "
//            << accessQualifier << bufferKeyword << " " << name << " {\n";
//
//        for (const auto& field : fields) {
//            ss << "    " << field.GenerateGLSLDeclaration() << ";\n";
//        }
//
//        ss << "}";
//
//        if (useInstanceName) {
//            std::string instanceName = name;
//            if (!instanceName.empty()) {
//                instanceName[0] = std::tolower(instanceName[0]);
//            }
//            ss << " " << instanceName;
//        }
//
//        ss << ";";
//
//        return ss.str();
//    }
//
//    json BufferObjectDefinition::ToJson() const {
//        json result;
//        result["name"] = name;
//        result["size"] = totalSize;
//        result["bufferType"] = static_cast<uint8_t>(bufferType);
//        result["layoutStandard"] = static_cast<uint8_t>(layoutStandard);
//        result["accessMode"] = static_cast<uint8_t>(bufferAccessMode);
//        result["useInstanceName"] = useInstanceName;
//
//        json fieldsJson = json::array();
//        for (const auto& field : fields) {
//            fieldsJson.push_back(field.ToJson());
//        }
//        result["fields"] = fieldsJson;
//
//        return result;
//    }
//
//    std::shared_ptr<BufferObjectDefinition> BufferObjectDefinition::FromJson(const json& j) {
//        std::string bufferName = j.at("name").get<std::string>();
//        BufferType type = static_cast<BufferType>(j.at("bufferType").get<uint8_t>());
//        LayoutStandard standard = static_cast<LayoutStandard>(j.at("layoutStandard").get<uint8_t>());
//        BufferAccessMode accessMode = static_cast<BufferAccessMode>(j.at("accessMode").get<uint8_t>());
//        bool useInstance = j.at("useInstanceName").get<bool>();
//
//        std::vector<BufferFieldDefinition> bufferFields;
//        for (const auto& fieldJson : j.at("fields")) {
//            bufferFields.push_back(BufferFieldDefinition::FromJson(fieldJson));
//        }
//
//        return std::make_shared<BufferObjectDefinition>(
//            bufferName, type, standard, accessMode, bufferFields, useInstance
//        );
//    }
//
//    std::shared_ptr<BufferObjectInstance> BufferObjectDefinition::CreateInstance() const {
//        return std::make_shared<BufferObjectInstance>(shared_from_this());
//    }
//
//    bool BufferObjectDefinition::Validate() const {
//        if (name.empty()) return false;
//        if (totalSize == 0) return false;
//        if (fields.empty()) return false;
//
//        // Validate field offsets are monotonically increasing
//        for (size_t i = 1; i < fields.size(); ++i) {
//            if (fields[i].offset <= fields[i - 1].offset) {
//                return false;
//            }
//        }
//
//        // Validate access modes
//        if (bufferType == BufferType::Uniform) {
//            if (bufferAccessMode != BufferAccessMode::ReadOnly) {
//                return false;
//            }
//            for (const auto& field : fields) {
//                if (!field.IsReadOnly()) {
//                    return false;
//                }
//            }
//        }
//
//        return true;
//    }
//
//    // ============================================================================
//    // BufferObjectBuilder Implementation
//    // ============================================================================
//
//    BufferObjectBuilder::BufferObjectBuilder(const std::string& bufferName)
//        : name(bufferName)
//        , bufferType(BufferType::Uniform)
//        , layoutStandard(LayoutStandard::Std140)
//        , defaultAccessMode(BufferAccessMode::ReadOnly) {
//    }
//
//    BufferObjectBuilder::BufferObjectBuilder(const std::string& bufferName,
//        BufferType type,
//        BufferAccessMode defaultAccess,
//        LayoutStandard standard)
//        : name(bufferName)
//        , bufferType(type)
//        , layoutStandard(standard)
//        , defaultAccessMode(defaultAccess) {
//
//        if (type == BufferType::Uniform) {
//            defaultAccessMode = BufferAccessMode::ReadOnly;
//        }
//
//        if (type == BufferType::Storage && standard == LayoutStandard::Std140) {
//            layoutStandard = LayoutStandard::Std430;
//        }
//    }
//
//    BufferObjectBuilder& BufferObjectBuilder::SetBufferType(BufferType type) {
//        bufferType = type;
//        if (type == BufferType::Uniform) {
//            defaultAccessMode = BufferAccessMode::ReadOnly;
//            if (layoutStandard == LayoutStandard::Std430) {
//                layoutStandard = LayoutStandard::Std140;
//            }
//        }
//        else if (type == BufferType::Storage && layoutStandard == LayoutStandard::Std140) {
//            layoutStandard = LayoutStandard::Std430;
//        }
//        return *this;
//    }
//
//    BufferObjectBuilder& BufferObjectBuilder::SetDefaultAccessMode(BufferAccessMode mode) {
//        if (bufferType == BufferType::Uniform && mode != BufferAccessMode::ReadOnly) {
//            throw std::invalid_argument("Uniform buffers must be ReadOnly");
//        }
//        defaultAccessMode = mode;
//        return *this;
//    }
//
//    BufferObjectBuilder& BufferObjectBuilder::SetLayoutStandard(LayoutStandard standard) {
//        layoutStandard = standard;
//        return *this;
//    }
//
//    BufferObjectBuilder& BufferObjectBuilder::SetUseInstanceName(bool use) {
//        useInstanceName = use;
//        return *this;
//    }
//
//    BufferObjectBuilder& BufferObjectBuilder::AddField(const std::string& fieldName, BaseType type) {
//        return AddField(fieldName, type, defaultAccessMode);
//    }
//
//    BufferObjectBuilder& BufferObjectBuilder::AddField(const std::string& fieldName, BaseType type,
//        BufferAccessMode accessMode) {
//        if (type == BaseType::Unknown) {
//            throw std::invalid_argument("BaseType::Unknown is not valid");
//        }
//        if (type == BaseType::Struct || type == BaseType::Array) {
//            throw std::invalid_argument("Use AddCompositeField for structs/arrays");
//        }
//
//        ValidateAccessMode(accessMode);
//
//        const BaseTypeInfo& info = GetBaseTypeInfo(type);
//        uint32_t offset = AlignTo(GetCurrentSize(), info.GetAlignment(layoutStandard));
//
//        fields.emplace_back(fieldName, type, info.size, offset, accessMode);
//        return *this;
//    }
//
//    BufferObjectBuilder& BufferObjectBuilder::AddCompositeField(const std::string& fieldName,
//        std::shared_ptr<const CompositeTypeDefinition> compositeDefinition) {
//        return AddCompositeField(fieldName, compositeDefinition, defaultAccessMode);
//    }
//
//    BufferObjectBuilder& BufferObjectBuilder::AddCompositeField(const std::string& fieldName,
//        std::shared_ptr<const CompositeTypeDefinition> compositeDefinition,
//        BufferAccessMode accessMode) {
//        if (!compositeDefinition) {
//            throw std::invalid_argument("Composite type definition cannot be null");
//        }
//
//        ValidateAccessMode(accessMode);
//
//        uint32_t fieldAlignment = compositeDefinition->GetAlignment();
//        uint32_t offset = AlignTo(GetCurrentSize(), fieldAlignment);
//
//        fields.emplace_back(fieldName, compositeDefinition, offset, accessMode);
//        return *this;
//    }
//
//    std::shared_ptr<BufferObjectDefinition> BufferObjectBuilder::Build() {
//        BufferAccessMode finalAccessMode = ComputeBufferAccessMode();
//
//        return std::make_shared<BufferObjectDefinition>(
//            name,
//            bufferType,
//            layoutStandard,
//            finalAccessMode,
//            fields,
//            useInstanceName
//        );
//    }
//
//    uint32_t BufferObjectBuilder::GetCurrentSize() const {
//        if (fields.empty()) return 0;
//        const auto& last = fields.back();
//        return last.offset + last.size;
//    }
//
//    void BufferObjectBuilder::ValidateAccessMode(BufferAccessMode mode) const {
//        if (bufferType == BufferType::Uniform && mode != BufferAccessMode::ReadOnly) {
//            throw std::invalid_argument("Fields in Uniform buffers must be ReadOnly");
//        }
//    }
//
//    BufferAccessMode BufferObjectBuilder::ComputeBufferAccessMode() const {
//        if (bufferType == BufferType::Uniform) {
//            return BufferAccessMode::ReadOnly;
//        }
//
//        bool hasReadOnly = false;
//        bool hasWriteOnly = false;
//        bool hasReadWrite = false;
//
//        for (const auto& field : fields) {
//            if (field.IsReadOnly()) hasReadOnly = true;
//            if (field.IsWriteOnly()) hasWriteOnly = true;
//            if (field.IsReadWrite()) hasReadWrite = true;
//        }
//
//        if (hasReadWrite || (hasReadOnly && hasWriteOnly)) {
//            return BufferAccessMode::ReadWrite;
//        }
//
//        if (hasReadOnly && !hasWriteOnly) {
//            return BufferAccessMode::ReadOnly;
//        }
//
//        if (hasWriteOnly && !hasReadOnly) {
//            return BufferAccessMode::WriteOnly;
//        }
//
//        return BufferAccessMode::ReadWrite;
//    }
//
//} // namespace ShaderLib