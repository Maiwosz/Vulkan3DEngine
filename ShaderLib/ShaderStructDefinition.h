#pragma once
#include "ShaderTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <json.hpp>

using json = nlohmann::json;

namespace ShaderLib {

    // ============================================================================
    // SHADER STRUCT DEFINITION - Immutable type metadata
    // ============================================================================

    class ShaderStructDefinition : public CompositeTypeDefinition,
        public std::enable_shared_from_this<ShaderStructDefinition> {
    public:
        struct Field {
            std::string name;
            BaseType baseType;
            uint32_t offset;
            uint32_t size;
            std::shared_ptr<const CompositeTypeDefinition> composite;

            // Constructor for base types
            Field(const std::string& n, BaseType t, uint32_t off, uint32_t sz);

            // Constructor for composite types
            Field(const std::string& n, std::shared_ptr<const CompositeTypeDefinition> comp, uint32_t off);

            bool IsComposite() const { return composite != nullptr; }
        };

    private:
        std::string typeName;
        std::vector<Field> fields;
        uint32_t size;
        uint32_t alignment;
        LayoutStandard layoutStandard;
        bool finalized;

        const Field* FindField(const std::string& name) const;

    public:
        explicit ShaderStructDefinition(const std::string& name,
            LayoutStandard standard = LayoutStandard::Std140);

        // Building interface (only before finalization)
        ShaderStructDefinition& AddField(const std::string& name, BaseType type);
        ShaderStructDefinition& AddCompositeField(const std::string& name,
            std::shared_ptr<const CompositeTypeDefinition> composite);
        void Finalize();

        // CompositeTypeDefinition interface
        std::string GetTypeName() const override { return typeName; }
        uint32_t GetSize() const override { return size; }
        uint32_t GetAlignment() const override { return alignment; }
        LayoutStandard GetLayoutStandard() const override { return layoutStandard; }

        // Serialization
        json ToJson() const override;

        std::string GenerateGLSL() const override;
        bool IsStruct() const override { return true; }
        bool IsArray() const override { return false; }

        // Factory method
        std::shared_ptr<CompositeTypeInstance> CreateInstance() const override;

        // Static factory - creates definition from JSON
        static std::shared_ptr<ShaderStructDefinition> FromJson(const json& j);

        // Additional getters
        const std::vector<Field>& GetFields() const { return fields; }
        bool IsFinalized() const { return finalized; }

        friend class ShaderStructInstance;
    };

} // namespace ShaderLib