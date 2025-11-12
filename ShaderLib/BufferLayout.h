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
    // WAŻNE: Dla tablic tworzymy JEDEN deskryptor zamiast N deskryptorów.
    // Dostęp do elementów: descriptor.GetElementOffset(index)
    // 
    // Korzyści:
    // - O(1) zamiast O(N) pamięci dla tablic
    // - O(1) zamiast O(N) czasu konstrukcji
    // - O(1) zamiast O(N) serializacji JSON
    // - Perfektne dla milionowych tablic
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

        // All fields (flat hierarchy, ONE descriptor per array)
        const std::vector<FieldDescriptor>& GetAllFields() const {
            return m_allFields;
        }

        // Top-level fields only (O(1) cached)
        const std::vector<size_t>& GetTopLevelIndices() const {
            return m_topLevelIndices;
        }

        // Find field by path (O(1) hash map lookup)
        // Obsługuje zarówno "colors" jak i "colors[5]" - zwraca ten sam deskryptor
        const FieldDescriptor* FindField(const std::string& path) const;

        // Get field by index (O(1) direct access)
        const FieldDescriptor* GetFieldByIndex(size_t index) const;

        // ========================================================================
        // FIELD QUERIES
        // ========================================================================

        // Get top-level field names (O(1) cached)
        std::vector<std::string> GetTopLevelFieldNames() const;

        // Get all field paths (flat hierarchy)
        std::vector<std::string> GetAllFieldPaths() const;

        // Get all child paths for a given parent path
        // e.g. "transform" -> ["transform.position", "transform.rotation", ...]
        std::vector<std::string> GetChildPaths(const std::string& parentPath) const;

        // Check if field is an array
        bool IsArrayField(const std::string& path) const;

        // Check if field is a structure
        bool IsStructureField(const std::string& path) const;

        // Get immediate children of a structure field
        // e.g. "transform" -> ["position", "rotation", "scale"]
        std::vector<std::string> GetStructureChildren(const std::string& path) const;

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
        // LAYOUT COMPUTATION
        // ========================================================================
        void ComputeLayout();

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

        void AddStructTemplateFields(
            std::shared_ptr<const StructureDefinition> structDef,
            const std::string& arrayPath,
            int32_t arrayParentIndex,
            uint32_t baseOffset
        );

        std::string BuildPath(
            const std::string& parentPath,
            const std::string& fieldName
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

        std::string ExtractTopLevelName(const std::string& path) const;

        // ========================================================================
        // MEMBER DATA
        // ========================================================================

        // Core data
        std::shared_ptr<const StructureDefinition> m_structure;
        LayoutStandard m_standard;

        // Computed layout (ONE descriptor per array!)
        std::vector<FieldDescriptor> m_allFields;
        uint32_t m_totalSize;
        uint32_t m_alignment;

        // Fast lookup maps
        std::unordered_map<std::string_view, size_t> m_pathToIndex;
        std::vector<size_t> m_topLevelIndices;
    };

} // namespace ShaderLib
