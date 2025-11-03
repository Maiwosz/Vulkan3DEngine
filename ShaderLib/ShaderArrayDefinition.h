#pragma once
#include "ShaderTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <json.hpp>

using json = nlohmann::json;

namespace ShaderLib {

    // ============================================================================
    // SHADER ARRAY DEFINITION - Immutable type metadata
    // ============================================================================

    class ShaderArrayDefinition : public CompositeTypeDefinition,
        public std::enable_shared_from_this<ShaderArrayDefinition> {
    private:
        BaseType elementBaseType;
        std::shared_ptr<const CompositeTypeDefinition> elementComposite;
        uint32_t arrayCount;
        uint32_t elementSize;
        uint32_t elementStride;
        uint32_t totalSize;
        uint32_t alignment;
        LayoutStandard layoutStandard;

    public:
        // Constructor for base types
        ShaderArrayDefinition(BaseType elemType, uint32_t count,
            LayoutStandard standard = LayoutStandard::Std140);

        // Constructor for composite types
        ShaderArrayDefinition(std::shared_ptr<const CompositeTypeDefinition> elemType,
            uint32_t count,
            LayoutStandard standard = LayoutStandard::Std140);

        // CompositeTypeDefinition interface
        std::string GetTypeName() const override;
        uint32_t GetSize() const override { return totalSize; }
        uint32_t GetAlignment() const override { return alignment; }
        LayoutStandard GetLayoutStandard() const override { return layoutStandard; }
        std::string GenerateGLSL() const override;
        bool IsStruct() const override { return false; }
        bool IsArray() const override { return true; }

        // Serialization
        json ToJson() const override;

        // Factory method
        std::shared_ptr<CompositeTypeInstance> CreateInstance() const override;

        // Static factory - creates definition from JSON
        static std::shared_ptr<ShaderArrayDefinition> FromJson(const json& j);

        // Additional getters
        uint32_t GetArrayCount() const { return arrayCount; }
        uint32_t GetElementSize() const { return elementSize; }
        uint32_t GetElementStride() const { return elementStride; }
        bool IsCompositeElement() const { return elementComposite != nullptr; }
        BaseType GetElementBaseType() const { return elementBaseType; }
        std::shared_ptr<const CompositeTypeDefinition> GetElementComposite() const { return elementComposite; }
    };


} // namespace ShaderLib