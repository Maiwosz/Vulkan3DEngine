#pragma once
#include "ShaderTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
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

    // ============================================================================
    // SHADER STRUCT INSTANCE - Mutable instance data
    // ============================================================================

    class ShaderStructInstance : public CompositeTypeInstance,
        public std::enable_shared_from_this<ShaderStructInstance> {
    private:
        std::shared_ptr<const ShaderStructDefinition> definition;
        std::unordered_map<std::string, BufferValue> fieldValues;
        std::vector<uint8_t> buffer;

        void InitializeFieldDefaults();
        void WriteFieldToBuffer(const ShaderStructDefinition::Field& field, const BufferValue& value);
        void ReadFieldFromBuffer(const ShaderStructDefinition::Field& field);
        BufferValue ConvertCompositeToBufferValue(std::shared_ptr<CompositeTypeInstance> instance) const;

    public:
        explicit ShaderStructInstance(std::shared_ptr<const ShaderStructDefinition> def);

        // Copy constructor
        ShaderStructInstance(const ShaderStructInstance& other);
        ShaderStructInstance& operator=(const ShaderStructInstance& other);

        // CompositeTypeInstance interface
        std::shared_ptr<const CompositeTypeDefinition> GetDefinition() const override { return definition; }
        const std::vector<uint8_t>& GetRawBuffer() const override { return buffer; }
        bool WriteToBuffer(void* dst) const override;
        bool ReadFromBuffer(const void* src) override;
        std::shared_ptr<CompositeTypeInstance> Clone() const override { return std::make_shared<ShaderStructInstance>(*this); }
        json ToJson() const override;
        bool FromJson(const json& j) override;
        bool IsStruct() const override { return true; }
        bool IsArray() const override { return false; }

        // Field access
        void SetField(const std::string& fieldName, const BufferValue& value);
        void SetCompositeField(const std::string& fieldName, std::shared_ptr<CompositeTypeInstance> value);

        BufferValue GetField(const std::string& fieldName) const;
        std::shared_ptr<CompositeTypeInstance> GetCompositeField(const std::string& fieldName) const;

        bool HasField(const std::string& fieldName) const;

        // Convenience: direct access to definition
        std::shared_ptr<const ShaderStructDefinition> GetStructDefinition() const { return definition; }
    };

} // namespace ShaderLib