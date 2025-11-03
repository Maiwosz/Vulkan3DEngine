#include "pch.h"
#include "ShaderStructDefinition.h"
#include "ShaderStructInstance.h"
#include "ShaderArrayDefinition.h"
#include <sstream>
#include <algorithm>

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

} // namespace ShaderLib