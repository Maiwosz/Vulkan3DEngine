#include "pch.h"
#include "StructureDefinition.h"
#include <stdexcept>
#include <sstream>

namespace ShaderLib {

    // ============================================================================
    // FIELD BUILDER IMPLEMENTATION
    // ============================================================================

    StructureDefinition::FieldBuilder::FieldBuilder(
        StructureDefinition* parent,
        const std::string& name
    )
        : m_parent(parent)
    {
        m_field.name = name;
    }

    StructureDefinition::FieldBuilder& StructureDefinition::FieldBuilder::BaseType(
        ShaderLib::BaseType type
    ) {
        m_field.baseType = type;
        m_field.structDef = nullptr;
        return *this;
    }

    StructureDefinition::FieldBuilder& StructureDefinition::FieldBuilder::Structure(
        std::shared_ptr<const StructureDefinition> structDef
    ) {
        if (!structDef) {
            throw std::runtime_error("Structure definition cannot be null");
        }

        m_field.structDef = structDef;
        m_field.baseType = ShaderLib::BaseType::Unknown;
        return *this;
    }

    StructureDefinition::FieldBuilder& StructureDefinition::FieldBuilder::Array(
        uint32_t size
    ) {
        m_field.arraySize = size;
        return *this;
    }

    StructureDefinition::FieldBuilder& StructureDefinition::FieldBuilder::Access(
        BufferAccessMode mode
    ) {
        m_field.accessMode = mode;
        return *this;
    }

    StructureDefinition& StructureDefinition::FieldBuilder::End() {
        m_parent->m_fields.push_back(m_field);
        return *m_parent;
    }

    // ============================================================================
    // STRUCTURE DEFINITION IMPLEMENTATION
    // ============================================================================

    StructureDefinition::StructureDefinition(const std::string& name)
        : m_name(name)
    {
    }

    StructureDefinition::FieldBuilder StructureDefinition::Field(const std::string& name) {
        return FieldBuilder(this, name);
    }

    StructureDefinition& StructureDefinition::AddField(
        const std::string& name,
        BaseType type,
        uint32_t arraySize,
        BufferAccessMode accessMode
    ) {
        FieldDef field;
        field.name = name;
        field.baseType = type;
        field.structDef = nullptr;
        field.arraySize = arraySize;
        field.accessMode = accessMode;

        m_fields.push_back(field);
        return *this;
    }

    StructureDefinition& StructureDefinition::AddField(
        const std::string& name,
        std::shared_ptr<const StructureDefinition> structDef,
        uint32_t arraySize,
        BufferAccessMode accessMode
    ) {
        if (!structDef) {
            throw std::runtime_error("Structure definition cannot be null");
        }

        FieldDef field;
        field.name = name;
        field.baseType = BaseType::Unknown;
        field.structDef = structDef;
        field.arraySize = arraySize;
        field.accessMode = accessMode;

        m_fields.push_back(field);
        return *this;
    }

    // ============================================================================
    // GLSL GENERATION
    // ============================================================================

    std::string StructureDefinition::GenerateGLSL() const {
        if (m_fields.empty()) {
            throw std::runtime_error("Cannot generate GLSL for empty structure");
        }

        std::stringstream ss;
        ss << "struct " << m_name << " {\n";

        GenerateGLSLRecursive(ss, 1);

        ss << "};";
        return ss.str();
    }

    void StructureDefinition::GenerateGLSLRecursive(
        std::stringstream& ss,
        int indentLevel
    ) const {
        std::string indent(indentLevel * 4, ' ');

        for (const auto& field : m_fields) {
            std::string typeName;

            if (field.isBaseType()) {
                typeName = BaseTypeToString(field.baseType);
            }
            else {
                typeName = field.structDef->GetName();
            }

            ss << indent << typeName << " " << field.name;

            if (field.isArray()) {
                ss << "[" << field.arraySize << "]";
            }

            ss << ";\n";
        }
    }

    // ============================================================================
    // SERIALIZATION
    // ============================================================================

    json StructureDefinition::ToJson() const {
        json j;
        j["name"] = m_name;

        json fieldsArray = json::array();
        for (const auto& field : m_fields) {
            json fieldJson;
            fieldJson["name"] = field.name;
            fieldJson["arraySize"] = field.arraySize;
            fieldJson["accessMode"] = static_cast<int>(field.accessMode);
            fieldJson["isBaseType"] = field.isBaseType();

            if (field.isBaseType()) {
                fieldJson["baseType"] = static_cast<int>(field.baseType);
            }
            else {
                fieldJson["structDef"] = field.structDef->ToJson();
            }

            fieldsArray.push_back(fieldJson);
        }
        j["fields"] = fieldsArray;

        return j;
    }

    std::shared_ptr<StructureDefinition> StructureDefinition::FromJson(const json& j) {
        std::string name = j.at("name").get<std::string>();
        auto def = std::make_shared<StructureDefinition>(name);

        if (j.contains("fields")) {
            for (const auto& fieldJson : j.at("fields")) {
                std::string fieldName = fieldJson.at("name").get<std::string>();
                uint32_t arraySize = fieldJson.at("arraySize").get<uint32_t>();
                BufferAccessMode accessMode = static_cast<BufferAccessMode>(
                    fieldJson.at("accessMode").get<int>()
                    );
                bool isBaseType = fieldJson.at("isBaseType").get<bool>();

                if (isBaseType) {
                    BaseType baseType = static_cast<BaseType>(
                        fieldJson.at("baseType").get<int>()
                        );
                    def->AddField(fieldName, baseType, arraySize, accessMode);
                }
                else {
                    auto structDef = FromJson(fieldJson.at("structDef"));
                    def->AddField(fieldName, structDef, arraySize, accessMode);
                }
            }
        }

        return def;
    }

} // namespace ShaderLib
