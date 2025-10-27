#pragma once
#include <json.hpp>
#include "ShaderTypes.h"
#include "ShaderStruct.h"
#include "ShaderArray.h"
#include <memory>

namespace ShaderLib {
    using json = nlohmann::json;

    // ============================================================================
    // COMPOSITE TYPE SERIALIZATION - Structure definitions only, not data
    // ============================================================================

    // Forward declarations
    json CompositeToJson(const std::shared_ptr<CompositeType>& composite);
    std::shared_ptr<CompositeType> CompositeFromJson(const json& j);

    // ============================================================================
    // SHADER STRUCT SERIALIZATION
    // ============================================================================

    inline json StructToJson(const ShaderStruct& structType) {
        json j;
        j["type"] = "struct";
        j["name"] = structType.GetTypeName();
        j["layoutStandard"] = structType.GetLayoutStandard();
        j["size"] = structType.GetSize();
        j["alignment"] = structType.GetAlignment();

        json fields = json::array();
        for (const auto& field : structType.GetFields()) {
            json fieldJson;
            fieldJson["name"] = field.name;
            fieldJson["offset"] = field.offset;
            fieldJson["size"] = field.size;

            if (field.IsComposite()) {
                // Recursive serialization for nested composite types
                fieldJson["baseType"] = field.composite->IsStruct() ? "struct" : "array";
                fieldJson["composite"] = CompositeToJson(field.composite);
            }
            else {
                fieldJson["baseType"] = BaseTypeToString(field.baseType);
            }

            fields.push_back(fieldJson);
        }
        j["fields"] = fields;

        return j;
    }

    inline std::shared_ptr<ShaderStruct> StructFromJson(const json& j) {
        std::string name = j.at("name").get<std::string>();
        LayoutStandard standard = j.at("layoutStandard").get<LayoutStandard>();

        auto structType = std::make_shared<ShaderStruct>(name, standard);

        const json& fields = j.at("fields");
        for (const auto& fieldJson : fields) {
            std::string fieldName = fieldJson.at("name").get<std::string>();
            std::string baseTypeStr = fieldJson.at("baseType").get<std::string>();

            if (baseTypeStr == "struct" || baseTypeStr == "array") {
                // Composite field - recursive deserialization
                auto composite = CompositeFromJson(fieldJson.at("composite"));
                structType->AddCompositeField(fieldName, composite);
            }
            else {
                // Base type field
                BaseType baseType = StringToBaseType(baseTypeStr);

                // Use templated AddField based on BaseType
                switch (baseType) {
                case BaseType::Bool:
                    structType->AddField<bool>(fieldName);
                    break;
                case BaseType::Float:
                    structType->AddField<float>(fieldName);
                    break;
                case BaseType::Vec2:
                    structType->AddField<glm::vec2>(fieldName);
                    break;
                case BaseType::Vec3:
                    structType->AddField<glm::vec3>(fieldName);
                    break;
                case BaseType::Vec4:
                    structType->AddField<glm::vec4>(fieldName);
                    break;
                case BaseType::Int:
                    structType->AddField<int32_t>(fieldName);
                    break;
                case BaseType::IVec2:
                    structType->AddField<glm::ivec2>(fieldName);
                    break;
                case BaseType::IVec3:
                    structType->AddField<glm::ivec3>(fieldName);
                    break;
                case BaseType::IVec4:
                    structType->AddField<glm::ivec4>(fieldName);
                    break;
                case BaseType::UInt:
                    structType->AddField<uint32_t>(fieldName);
                    break;
                case BaseType::UVec2:
                    structType->AddField<glm::uvec2>(fieldName);
                    break;
                case BaseType::UVec3:
                    structType->AddField<glm::uvec3>(fieldName);
                    break;
                case BaseType::UVec4:
                    structType->AddField<glm::uvec4>(fieldName);
                    break;
                case BaseType::Double:
                    structType->AddField<double>(fieldName);
                    break;
                case BaseType::DVec2:
                    structType->AddField<glm::dvec2>(fieldName);
                    break;
                case BaseType::DVec3:
                    structType->AddField<glm::dvec3>(fieldName);
                    break;
                case BaseType::DVec4:
                    structType->AddField<glm::dvec4>(fieldName);
                    break;
                case BaseType::Mat2:
                    structType->AddField<glm::mat2>(fieldName);
                    break;
                case BaseType::Mat3:
                    structType->AddField<glm::mat3>(fieldName);
                    break;
                case BaseType::Mat4:
                    structType->AddField<glm::mat4>(fieldName);
                    break;
                default:
                    throw std::runtime_error("Unsupported base type in struct deserialization: " + baseTypeStr);
                }
            }
        }

        structType->Finalize();
        return structType;
    }

    // ============================================================================
    // SHADER ARRAY SERIALIZATION
    // ============================================================================

    inline json ArrayToJson(const ShaderArray& arrayType) {
        json j;
        j["type"] = "array";
        j["arrayCount"] = arrayType.GetArrayCount();
        j["layoutStandard"] = arrayType.GetLayoutStandard();
        j["size"] = arrayType.GetSize();
        j["alignment"] = arrayType.GetAlignment();
        j["elementSize"] = arrayType.GetElementSize();
        j["elementStride"] = arrayType.GetElementStride();

        if (arrayType.IsCompositeElement()) {
            // Composite element type
            j["elementType"] = arrayType.GetElementComposite()->IsStruct() ? "struct" : "array";
            j["elementComposite"] = CompositeToJson(arrayType.GetElementComposite());
        }
        else {
            // Base element type
            j["elementType"] = BaseTypeToString(arrayType.GetElementBaseType());
        }

        return j;
    }

    inline std::shared_ptr<ShaderArray> ArrayFromJson(const json& j) {
        uint32_t arrayCount = j.at("arrayCount").get<uint32_t>();
        LayoutStandard standard = j.at("layoutStandard").get<LayoutStandard>();
        std::string elementTypeStr = j.at("elementType").get<std::string>();

        if (elementTypeStr == "struct" || elementTypeStr == "array") {
            // Composite element type
            auto elementComposite = CompositeFromJson(j.at("elementComposite"));
            return std::make_shared<ShaderArray>(elementComposite, arrayCount, standard);
        }
        else {
            // Base element type
            BaseType elementType = StringToBaseType(elementTypeStr);
            return std::make_shared<ShaderArray>(elementType, arrayCount, standard);
        }
    }

    // ============================================================================
    // GENERIC COMPOSITE TYPE SERIALIZATION
    // ============================================================================

    inline json CompositeToJson(const std::shared_ptr<CompositeType>& composite) {
        if (!composite) {
            return json(nullptr);
        }

        if (composite->IsStruct()) {
            auto structPtr = std::dynamic_pointer_cast<ShaderStruct>(composite);
            if (!structPtr) {
                throw std::runtime_error("Failed to cast CompositeType to ShaderStruct");
            }
            return StructToJson(*structPtr);
        }
        else if (composite->IsArray()) {
            auto arrayPtr = std::dynamic_pointer_cast<ShaderArray>(composite);
            if (!arrayPtr) {
                throw std::runtime_error("Failed to cast CompositeType to ShaderArray");
            }
            return ArrayToJson(*arrayPtr);
        }

        throw std::runtime_error("Unknown composite type");
    }

    inline std::shared_ptr<CompositeType> CompositeFromJson(const json& j) {
        if (j.is_null()) {
            return nullptr;
        }

        std::string type = j.at("type").get<std::string>();

        if (type == "struct") {
            return StructFromJson(j);
        }
        else if (type == "array") {
            return ArrayFromJson(j);
        }

        throw std::runtime_error("Unknown composite type: " + type);
    }

} // namespace ShaderLib