#pragma once
#include "ShaderTypes.h"
#include <string>
#include <vector>
#include <memory>
#include "BufferAccessPatterns.h"

namespace ShaderLib {

    // ============================================================================
    // STRUCTURE DEFINITION - Hierarchiczna definicja struktury (bez layoutu)
    // 
    // Służy TYLKO do definicji struktury - nie zawiera offsetów, paddingu, etc.
    // Builder pattern dla wygodnego definiowania zagnieżdżonych struktur.
    // Może reprezentować zarówno struct jak i całą strukturę bufora.
    // ============================================================================

    class StructureDefinition : public std::enable_shared_from_this<StructureDefinition> {
    public:
        // ========================================================================
        // FIELD DEFINITION - Pojedyncze pole w strukturze
        // ========================================================================
        struct FieldDef {
            std::string name;

            // Type info
            BaseType baseType;  // For base types
            std::shared_ptr<const StructureDefinition> structDef;  // For nested structures

            // Array info
            uint32_t arraySize;  // 0 = not array, >0 = array size

            // Access mode
            AccessOperation accessOperation;

            // Helpers
            bool isBaseType() const { return structDef == nullptr; }
            bool isArray() const { return arraySize > 0; }

            FieldDef()
                : baseType(BaseType::Unknown)
                , structDef(nullptr)
                , arraySize(0)
                , accessOperation(AccessOperation::ReadWrite)
            {
            }
        };

        // ========================================================================
        // NESTED FIELD BUILDER
        // ========================================================================
        class FieldBuilder {
        public:
            FieldBuilder(StructureDefinition* parent, const std::string& name);

            FieldBuilder& BaseType(ShaderLib::BaseType type);
            FieldBuilder& Structure(std::shared_ptr<const StructureDefinition> structDef);
            FieldBuilder& Array(uint32_t size);
            FieldBuilder& Access(AccessOperation operation);

            StructureDefinition& End();

        private:
            StructureDefinition* m_parent;
            FieldDef m_field;
        };

        // ========================================================================
        // CONSTRUCTION
        // ========================================================================
        explicit StructureDefinition(const std::string& name);

        // Fluent API
        FieldBuilder Field(const std::string& name);

        // Direct field addition
        StructureDefinition& AddField(
            const std::string& name,
            BaseType type,
            uint32_t arraySize = 0,
            AccessOperation accessOperation = AccessOperation::ReadWrite
        );

        StructureDefinition& AddField(
            const std::string& name,
            std::shared_ptr<const StructureDefinition> structDef,
            uint32_t arraySize = 0,
            AccessOperation accessOperation = AccessOperation::ReadWrite
        );

        // ========================================================================
        // BASIC INFO
        // ========================================================================
        const std::string& GetName() const { return m_name; }
        const std::vector<FieldDef>& GetFields() const { return m_fields; }
        bool IsEmpty() const { return m_fields.empty(); }

        // ========================================================================
        // GLSL GENERATION (structure only, no layout qualifiers)
        // ========================================================================
        std::string GenerateGLSL() const;

        // ========================================================================
        // SERIALIZATION
        // ========================================================================
        json ToJson() const;
        static std::shared_ptr<StructureDefinition> FromJson(const json& j);

    private:
        void GenerateGLSLRecursive(std::stringstream& ss, int indentLevel) const;

        std::string m_name;
        std::vector<FieldDef> m_fields;

        friend class FieldBuilder;
    };

    // ============================================================================
    // FACTORY HELPER
    // ============================================================================
    inline std::shared_ptr<StructureDefinition> MakeStruct(const std::string& name) {
        return std::make_shared<StructureDefinition>(name);
    }

} // namespace ShaderLib
