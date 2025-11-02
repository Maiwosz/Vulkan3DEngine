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

    // ============================================================================
    // SHADER ARRAY INSTANCE - Mutable instance data
    // ============================================================================

    class ShaderArrayInstance : public CompositeTypeInstance,
        public std::enable_shared_from_this<ShaderArrayInstance> {
    private:
        std::shared_ptr<const ShaderArrayDefinition> definition;
        std::vector<BufferValue> elements;
        std::vector<uint8_t> buffer;

        void ValidateIndex(uint32_t index) const;
        void InitializeElementDefaults();
        void WriteElementToBuffer(uint32_t index, const BufferValue& value);
        void ReadElementFromBuffer(uint32_t index);
        BufferValue ConvertCompositeToBufferValue(std::shared_ptr<CompositeTypeInstance> instance) const;

    public:
        explicit ShaderArrayInstance(std::shared_ptr<const ShaderArrayDefinition> def);

        // Copy constructor
        ShaderArrayInstance(const ShaderArrayInstance& other);
        ShaderArrayInstance& operator=(const ShaderArrayInstance& other);

        // CompositeTypeInstance interface
        std::shared_ptr<const CompositeTypeDefinition> GetDefinition() const override { return definition; }
        const std::vector<uint8_t>& GetRawBuffer() const override { return buffer; }
        bool WriteToBuffer(void* dst) const override;
        bool ReadFromBuffer(const void* src) override;
        std::shared_ptr<CompositeTypeInstance> Clone() const override { return std::make_shared<ShaderArrayInstance>(*this); }
        json ToJson() const override;
        bool FromJson(const json& j) override;
        bool IsStruct() const override { return false; }
        bool IsArray() const override { return true; }

        // Element access
        void SetElement(uint32_t index, const BufferValue& value);
        void SetCompositeElement(uint32_t index, std::shared_ptr<CompositeTypeInstance> value);

        BufferValue GetElement(uint32_t index) const;
        std::shared_ptr<CompositeTypeInstance> GetCompositeElement(uint32_t index) const;

        // Convenience: direct access to definition
        std::shared_ptr<const ShaderArrayDefinition> GetArrayDefinition() const;
    };

} // namespace ShaderLib