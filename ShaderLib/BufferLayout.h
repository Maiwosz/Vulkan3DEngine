#pragma once
#include "StructureDefinition.h"
#include "FieldDescriptor.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string_view>
#include <sstream>

namespace ShaderLib {

    // ============================================================================
    // BUFFER LAYOUT - Płaski layout pamięci z offsetami i paddingiem
    // 
    // Przyjmuje StructureDefinition i LayoutStandard, następnie:
    // - Oblicza offsety, padding, alignment dla wszystkich pól
    // - Spłaszcza hierarchię (każde pole ma pełną ścieżkę)
    // - Buduje mapy dla szybkiego dostępu O(1)
    // - Każde pole zna swojego rodzica (przez indeks w m_allFields)
    // 
    // WAŻNE: FieldDescriptor używa structTypeName (string) zamiast structDef (shared_ptr)
    // bo layout jest już płaski i nie potrzebuje zagnieżdżonych definicji.
    // Oryginalną StructureDefinition zachowujemy w m_structure dla GLSL generation.
    // 
    // Jest IMMUTABLE po konstrukcji - wszystko precomputowane.
    // ============================================================================

    class BufferLayout {
    public:
        // ========================================================================
        // CONSTRUCTION
        // ========================================================================
        BufferLayout(
            std::shared_ptr<const StructureDefinition> structure,
            LayoutStandard standard
        );

        // ========================================================================
        // BASIC INFO
        // ========================================================================
        std::shared_ptr<const StructureDefinition> GetStructure() const {
            return m_structure;
        }

        LayoutStandard GetLayoutStandard() const { return m_standard; }
        uint32_t GetTotalSize() const { return m_totalSize; }
        uint32_t GetAlignment() const { return m_alignment; }

        // ========================================================================
        // FIELD ACCESS
        // ========================================================================

        // All fields (flat hierarchy)
        const std::vector<FieldDescriptor>& GetAllFields() const {
            return m_allFields;
        }

        // Top-level fields only (O(1) cached)
        const std::vector<size_t>& GetTopLevelIndices() const {
            return m_topLevelIndices;
        }

        // Find field by path (O(1) hash map lookup)
        const FieldDescriptor* FindField(const std::string& path) const;

        // Get field by index (O(1) direct access)
        const FieldDescriptor* GetFieldByIndex(size_t index) const;

        // ========================================================================
        // DEBUG UTILITIES
        // ========================================================================

        // Print detailed layout information to string
        std::string DebugPrint(bool showPadding = true) const;

        // Print layout to stdout
        void DebugPrintToConsole(bool showPadding = true) const;

        // Validate layout correctness
        struct ValidationResult {
            bool isValid;
            std::vector<std::string> errors;
            std::vector<std::string> warnings;
        };
        ValidationResult Validate() const;

        // ========================================================================
        // SERIALIZATION
        // ========================================================================
        json ToJson() const;
        static std::shared_ptr<BufferLayout> FromJson(const json& j);

    private:
        // ========================================================================
        // LAYOUT COMPUTATION - Top level entry point
        // ========================================================================
        void ComputeLayout();

        // ========================================================================
        // UNIFIED FIELD PROCESSING - Handles both top-level and nested fields
        // ========================================================================
        uint32_t ProcessField(
            const StructureDefinition::FieldDef& fieldDef,
            const std::string& parentPath,
            int32_t parentIndex,
            uint32_t currentOffset
        );

        uint32_t ProcessBaseType(
            const StructureDefinition::FieldDef& fieldDef,
            const std::string& parentPath,
            int32_t parentIndex,
            uint32_t currentOffset
        );

        uint32_t ProcessArray(
            const StructureDefinition::FieldDef& fieldDef,
            const std::string& parentPath,
            int32_t parentIndex,
            uint32_t currentOffset
        );

        uint32_t ProcessStruct(
            const StructureDefinition::FieldDef& fieldDef,
            const std::string& parentPath,
            int32_t parentIndex,
            uint32_t currentOffset
        );

        void AddNestedStructFields(
            std::shared_ptr<const StructureDefinition> structDef,
            const std::string& structPath,
            int32_t structParentIndex,
            uint32_t baseOffset
        );

        void AddArrayElementDescriptor(
            const StructureDefinition::FieldDef& fieldDef,
            const std::string& parentPath,
            int32_t parentIndex,
            uint32_t elementOffset,
            uint32_t elementRelativeOffset,
            uint32_t arrayAlignment,
            uint32_t stride,
            uint32_t arrayIndex
        );

        void AddStructArrayElementDescriptor(
            const StructureDefinition::FieldDef& fieldDef,
            const std::string& parentPath,
            int32_t parentIndex,
            uint32_t elementOffset,
            uint32_t elementRelativeOffset,
            uint32_t elementSize,
            uint32_t arrayAlignment,
            uint32_t stride,
            uint32_t arrayIndex
        );

        std::string BuildPath(
            const std::string& parentPath,
            const std::string& fieldName,
            bool isArrayElement,
            uint32_t arrayIndex
        ) const;

        // ========================================================================
        // HELPER METHODS
        // ========================================================================
        void BuildMaps();

        // Debug helpers
        std::string GetTypeString(const FieldDescriptor& field) const;
        void AppendFieldDebugInfo(
            std::stringstream& ss,
            const FieldDescriptor& field,
            int indentLevel,
            bool showPadding,
            uint32_t* lastOffset
        ) const;

        // ========================================================================
        // MEMBER DATA
        // ========================================================================

        // Core data
        std::shared_ptr<const StructureDefinition> m_structure;
        LayoutStandard m_standard;

        // Computed layout
        std::vector<FieldDescriptor> m_allFields;  // Flat hierarchy
        uint32_t m_totalSize;
        uint32_t m_alignment;

        // Fast lookup maps
        std::unordered_map<std::string_view, size_t> m_pathToIndex;
        std::vector<size_t> m_topLevelIndices;
    };

} // namespace ShaderLib
