#include "pch.h"
#include "ShaderArrayDefinition.h"
#include "ShaderArrayInstance.h"
#include <sstream>
#include <stdexcept>

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

} // namespace ShaderLib