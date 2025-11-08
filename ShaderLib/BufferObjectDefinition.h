#pragma once
#include "BufferLayout.h"
#include <string>
#include <memory>
#include <set>

namespace ShaderLib {

    class BufferObjectInstance;

    // ============================================================================
    // BUFFER TYPE
    // ============================================================================

    enum class BufferType {
        Uniform,  // UBO - always ReadOnly, std140
        Storage   // SSBO - can be ReadOnly/WriteOnly/ReadWrite, prefer std430
    };

    // ============================================================================
    // BUFFER OBJECT DEFINITION
    // 
    // Contains:
    // - BufferLayout (structure + computed layout)
    // - Buffer-specific metadata (type, GLSL generation)
    // 
    // Immutable after construction.
    // ============================================================================

    class BufferObjectDefinition : public std::enable_shared_from_this<BufferObjectDefinition> {
    public:
        // ========================================================================
        // CONSTRUCTION
        // ========================================================================

        BufferObjectDefinition(
            std::shared_ptr<BufferLayout> layout,
            BufferType bufferType = BufferType::Uniform
        );

        // Convenience constructor
        BufferObjectDefinition(
            std::shared_ptr<const StructureDefinition> structure,
            BufferType bufferType = BufferType::Uniform,
            LayoutStandard layoutStandard = LayoutStandard::Std140
        );

        // ========================================================================
        // LAYOUT ACCESS - Full delegation to BufferLayout
        // ========================================================================

        std::shared_ptr<const BufferLayout> GetLayout() const { return m_layout; }

        // Direct delegation
        const std::string& GetName() const {
            return m_layout->GetStructure()->GetName();
        }

        LayoutStandard GetLayoutStandard() const {
            return m_layout->GetLayoutStandard();
        }

        uint32_t GetTotalSize() const {
            return m_layout->GetTotalSize();
        }

        uint32_t GetAlignment() const {
            return m_layout->GetAlignment();
        }

        const std::vector<FieldDescriptor>& GetAllFields() const {
            return m_layout->GetAllFields();
        }

        const FieldDescriptor* FindField(const std::string& path) const {
            return m_layout->FindField(path);
        }

        // ========================================================================
        // BUFFER-SPECIFIC PROPERTIES
        // ========================================================================

        BufferType GetBufferType() const { return m_bufferType; }
        bool IsUniformBuffer() const { return m_bufferType == BufferType::Uniform; }
        bool IsStorageBuffer() const { return m_bufferType == BufferType::Storage; }

        bool UseInstanceName() const { return m_useInstanceName; }
        void SetUseInstanceName(bool use) { m_useInstanceName = use; }

        // ========================================================================
        // ACCESS MODE ANALYSIS
        // ========================================================================

        BufferAccessMode ComputeEffectiveAccessMode() const;

        bool IsReadOnly() const {
            return m_bufferType == BufferType::Uniform;
        }

        bool IsWriteOnly() const {
            return m_bufferType == BufferType::Storage &&
                ComputeEffectiveAccessMode() == BufferAccessMode::WriteOnly;
        }

        bool IsReadWrite() const {
            return m_bufferType == BufferType::Storage &&
                ComputeEffectiveAccessMode() == BufferAccessMode::ReadWrite;
        }

        // ========================================================================
        // VALIDATION
        // ========================================================================

        void ValidateBufferConfiguration() const;

        // ========================================================================
        // GLSL GENERATION
        // ========================================================================

        std::string GenerateBufferGLSL(uint32_t set, uint32_t binding) const;
        std::string GetAccessQualifier() const;

        void CollectNestedStructDefinitions(
            std::set<std::string>& outStructDefs,
            std::set<std::string>& processedNames
        ) const;

        // ========================================================================
        // INSTANCE FACTORY
        // ========================================================================

        std::shared_ptr<BufferObjectInstance> CreateInstance() const;

        // ========================================================================
        // SERIALIZATION
        // ========================================================================

        json ToJson() const;
        static std::shared_ptr<BufferObjectDefinition> FromJson(const json& j);

    private:
        void CollectNestedStructDefinitionsRecursive(
            std::shared_ptr<const StructureDefinition> structDef,
            std::set<std::string>& outStructDefs,
            std::set<std::string>& processedNames
        ) const;

        std::shared_ptr<const BufferLayout> m_layout;
        BufferType m_bufferType;
        bool m_useInstanceName;
    };

    // ============================================================================
    // FACTORY HELPERS
    // ============================================================================

    inline std::shared_ptr<BufferObjectDefinition> MakeUniformBuffer(
        std::shared_ptr<const StructureDefinition> structure
    ) {
        return std::make_shared<BufferObjectDefinition>(
            structure,
            BufferType::Uniform,
            LayoutStandard::Std140
        );
    }

    inline std::shared_ptr<BufferObjectDefinition> MakeStorageBuffer(
        std::shared_ptr<const StructureDefinition> structure,
        LayoutStandard layoutStandard = LayoutStandard::Std430
    ) {
        return std::make_shared<BufferObjectDefinition>(
            structure,
            BufferType::Storage,
            layoutStandard
        );
    }

} // namespace ShaderLib
