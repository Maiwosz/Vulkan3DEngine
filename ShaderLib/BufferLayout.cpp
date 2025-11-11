#include "pch.h"
#include "BufferLayout.h"
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <iostream>

namespace ShaderLib {

    // ============================================================================
    // HELPERS FOR STRUCT LAYOUT COMPUTATION
    // ============================================================================

    struct StructLayoutInfo {
        uint32_t size;
        uint32_t alignment;
    };

    static StructLayoutInfo ComputeStructLayout(
        std::shared_ptr<const StructureDefinition> structDef,
        LayoutStandard standard
    ) {
        uint32_t maxAlignment = 0;
        uint32_t currentOffset = 0;

        for (const auto& field : structDef->GetFields()) {
            uint32_t fieldAlignment;
            uint32_t fieldSize;

            if (field.isArray()) {
                // Array in struct
                if (field.isBaseType()) {
                    const BaseTypeInfo& typeInfo = GetBaseTypeInfo(field.baseType);
                    fieldSize = typeInfo.size;
                    fieldAlignment = typeInfo.GetAlignment(standard);
                }
                else {
                    // Array of structs
                    StructLayoutInfo nestedInfo = ComputeStructLayout(field.structDef, standard);
                    fieldSize = nestedInfo.size;
                    fieldAlignment = nestedInfo.alignment;
                }

                // Array stride
                uint32_t stride;
                if (standard == LayoutStandard::Std140) {
                    uint32_t arrayAlignment = std::max(fieldAlignment, 16u);
                    stride = AlignTo(fieldSize, arrayAlignment);
                    fieldAlignment = arrayAlignment;
                }
                else {
                    stride = AlignTo(fieldSize, fieldAlignment);
                }

                currentOffset = AlignTo(currentOffset, fieldAlignment);
                uint32_t arraySize = field.arraySize * stride;
                currentOffset += arraySize;

                maxAlignment = std::max(maxAlignment, fieldAlignment);
            }
            else if (field.isBaseType()) {
                const BaseTypeInfo& typeInfo = GetBaseTypeInfo(field.baseType);
                fieldAlignment = typeInfo.GetAlignment(standard);
                fieldSize = typeInfo.size;

                currentOffset = AlignTo(currentOffset, fieldAlignment);
                currentOffset += fieldSize;

                maxAlignment = std::max(maxAlignment, fieldAlignment);
            }
            else {
                StructLayoutInfo nestedInfo = ComputeStructLayout(field.structDef, standard);
                fieldAlignment = nestedInfo.alignment;
                fieldSize = nestedInfo.size;

                currentOffset = AlignTo(currentOffset, fieldAlignment);
                currentOffset += fieldSize;

                maxAlignment = std::max(maxAlignment, fieldAlignment);
            }
        }

        // Struct alignment rules
        if (standard == LayoutStandard::Std140) {
            maxAlignment = AlignTo(maxAlignment, 16u);
        }

        uint32_t structSize = AlignTo(currentOffset, maxAlignment);

        return { structSize, maxAlignment };
    }

    // ============================================================================
    // CONSTRUCTION
    // ============================================================================

    BufferLayout::BufferLayout(
        std::shared_ptr<const StructureDefinition> structure,
        LayoutStandard standard
    )
        : m_structure(structure)
        , m_standard(standard)
        , m_totalSize(0)
        , m_alignment(0)
    {
        if (!structure) {
            throw std::runtime_error("Structure definition cannot be null");
        }

        if (structure->IsEmpty()) {
            throw std::runtime_error("Cannot create layout from empty structure");
        }

        ComputeLayout();
        BuildMaps();
    }

    // ============================================================================
    // LAYOUT COMPUTATION
    // ============================================================================

    void BufferLayout::ComputeLayout() {
        m_allFields.clear();
        m_alignment = 0;

        uint32_t currentOffset = 0;
        const auto& fields = m_structure->GetFields();

        for (const auto& fieldDef : fields) {
            currentOffset = ProcessField(fieldDef, "", -1, currentOffset);
        }

        // Buffer-level alignment rules
        if (m_standard == LayoutStandard::Std140) {
            m_alignment = std::max(m_alignment, 16u);
        }

        m_totalSize = AlignTo(currentOffset, m_alignment);
    }

    // ============================================================================
    // UNIFIED FIELD PROCESSING
    // ============================================================================

    uint32_t BufferLayout::ProcessField(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t currentOffset
    ) {
        if (fieldDef.isArray()) {
            return ProcessArray(fieldDef, parentPath, parentIndex, currentOffset);
        }
        else if (fieldDef.isBaseType()) {
            return ProcessBaseType(fieldDef, parentPath, parentIndex, currentOffset);
        }
        else {
            return ProcessStruct(fieldDef, parentPath, parentIndex, currentOffset);
        }
    }

    uint32_t BufferLayout::ProcessBaseType(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t currentOffset
    ) {
        const BaseTypeInfo& typeInfo = GetBaseTypeInfo(fieldDef.baseType);
        const uint32_t alignment = typeInfo.GetAlignment(m_standard);

        currentOffset = AlignTo(currentOffset, alignment);

        uint32_t relativeOffset = currentOffset;
        if (parentIndex >= 0) {
            relativeOffset = currentOffset - m_allFields[parentIndex].offset;
        }

        FieldDescriptor desc;
        desc.name = fieldDef.name;
        desc.path = BuildPath(parentPath, fieldDef.name, false, 0);
        desc.baseType = fieldDef.baseType;
        desc.structTypeName.clear();
        desc.arraySize = 0;
        desc.arrayIndex = 0;
        desc.offset = currentOffset;
        desc.relativeOffset = relativeOffset;
        desc.size = typeInfo.size;
        desc.alignment = alignment;
        desc.stride = 0;
        desc.parentPath = parentPath;
        desc.parentIndex = parentIndex;
        desc.isBaseType = true;
        desc.isArray = false;
        desc.isArrayElement = false;
        desc.accessMode = fieldDef.accessMode;

        m_allFields.push_back(desc);
        m_alignment = std::max(m_alignment, alignment);

        return currentOffset + typeInfo.size;
    }

    uint32_t BufferLayout::ProcessArray(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t currentOffset
    ) {
        // Get element info
        uint32_t elementSize;
        uint32_t elementAlignment;

        if (fieldDef.isBaseType()) {
            const BaseTypeInfo& typeInfo = GetBaseTypeInfo(fieldDef.baseType);
            elementSize = typeInfo.size;
            elementAlignment = typeInfo.GetAlignment(m_standard);
        }
        else {
            StructLayoutInfo structInfo = ComputeStructLayout(fieldDef.structDef, m_standard);
            elementSize = structInfo.size;
            elementAlignment = structInfo.alignment;
        }

        // Calculate array stride and alignment
        uint32_t stride;
        uint32_t arrayAlignment;

        if (m_standard == LayoutStandard::Std140) {
            arrayAlignment = std::max(elementAlignment, 16u);
            stride = AlignTo(elementSize, arrayAlignment);
        }
        else {
            arrayAlignment = elementAlignment;
            stride = AlignTo(elementSize, elementAlignment);
        }

        currentOffset = AlignTo(currentOffset, arrayAlignment);
        const uint32_t arrayStart = currentOffset;

        uint32_t baseRelativeOffset = arrayStart;
        if (parentIndex >= 0) {
            baseRelativeOffset = arrayStart - m_allFields[parentIndex].offset;
        }

        // NOWE: Dodaj deskryptor dla całej tablicy (bez indeksu)
        AddArrayContainerDescriptor(
            fieldDef, parentPath, parentIndex,
            arrayStart, baseRelativeOffset,
            arrayAlignment, stride, elementSize
        );

        // Process each array element
        for (uint32_t i = 0; i < fieldDef.arraySize; ++i) {
            const uint32_t elementOffset = arrayStart + (i * stride);
            const uint32_t elementRelativeOffset = baseRelativeOffset + (i * stride);

            if (fieldDef.isBaseType()) {
                AddArrayElementDescriptor(
                    fieldDef, parentPath, parentIndex,
                    elementOffset, elementRelativeOffset,
                    arrayAlignment, stride, i
                );
            }
            else {
                AddStructArrayElementDescriptor(
                    fieldDef, parentPath, parentIndex,
                    elementOffset, elementRelativeOffset, elementSize,
                    arrayAlignment, stride, i
                );
            }
        }

        m_alignment = std::max(m_alignment, arrayAlignment);

        return arrayStart + (fieldDef.arraySize * stride);
    }

    uint32_t BufferLayout::ProcessStruct(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t currentOffset
    ) {
        StructLayoutInfo structInfo = ComputeStructLayout(fieldDef.structDef, m_standard);

        const uint32_t structSize = structInfo.size;
        const uint32_t structAlignment = structInfo.alignment;

        currentOffset = AlignTo(currentOffset, structAlignment);

        uint32_t relativeOffset = currentOffset;
        if (parentIndex >= 0) {
            relativeOffset = currentOffset - m_allFields[parentIndex].offset;
        }

        const std::string structPath = BuildPath(parentPath, fieldDef.name, false, 0);
        const int32_t structDescIndex = static_cast<int32_t>(m_allFields.size());

        // Add struct descriptor
        FieldDescriptor structDesc;
        structDesc.name = fieldDef.name;
        structDesc.path = structPath;
        structDesc.baseType = BaseType::Unknown;
        structDesc.structTypeName = fieldDef.structDef->GetName();
        structDesc.arraySize = 0;
        structDesc.arrayIndex = 0;
        structDesc.offset = currentOffset;
        structDesc.relativeOffset = relativeOffset;
        structDesc.size = structSize;
        structDesc.alignment = structAlignment;
        structDesc.stride = 0;
        structDesc.parentPath = parentPath;
        structDesc.parentIndex = parentIndex;
        structDesc.isBaseType = false;
        structDesc.isArray = false;
        structDesc.isArrayElement = false;
        structDesc.accessMode = fieldDef.accessMode;

        m_allFields.push_back(structDesc);

        // Recursively add nested fields
        AddNestedStructFields(fieldDef.structDef, structPath, structDescIndex, currentOffset);

        m_alignment = std::max(m_alignment, structAlignment);

        return currentOffset + structSize;
    }

    // ============================================================================
    // HELPER METHODS
    // ============================================================================

    void BufferLayout::AddNestedStructFields(
        std::shared_ptr<const StructureDefinition> structDef,
        const std::string& structPath,
        int32_t structParentIndex,
        uint32_t baseOffset
    ) {
        uint32_t currentOffset = baseOffset;

        for (const auto& field : structDef->GetFields()) {
            currentOffset = ProcessField(field, structPath, structParentIndex, currentOffset);
        }
    }

    void BufferLayout::AddArrayContainerDescriptor(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t arrayStart,
        uint32_t arrayRelativeOffset,
        uint32_t arrayAlignment,
        uint32_t stride,
        uint32_t elementSize
    ) {
        // Ścieżka BEZ indeksu - reprezentuje całą tablicę
        const std::string arrayPath = BuildPath(parentPath, fieldDef.name, false, 0);

        // Całkowity rozmiar tablicy
        const uint32_t totalArraySize = fieldDef.arraySize * stride;

        FieldDescriptor desc;
        desc.name = fieldDef.name;
        desc.path = arrayPath;

        if (fieldDef.isBaseType()) {
            desc.baseType = fieldDef.baseType;
            desc.structTypeName.clear();
            desc.isBaseType = true;
        }
        else {
            desc.baseType = BaseType::Unknown;
            desc.structTypeName = fieldDef.structDef->GetName();
            desc.isBaseType = false;
        }

        desc.arraySize = fieldDef.arraySize;
        desc.arrayIndex = 0;  // Nie jest konkretnym elementem
        desc.offset = arrayStart;
        desc.relativeOffset = arrayRelativeOffset;
        desc.size = totalArraySize;  // Rozmiar całej tablicy!
        desc.alignment = arrayAlignment;
        desc.stride = stride;
        desc.parentPath = parentPath;
        desc.parentIndex = parentIndex;
        desc.isArray = true;
        desc.isArrayElement = false;  // To cała tablica, nie element!
        desc.accessMode = fieldDef.accessMode;

        m_allFields.push_back(desc);
    }

    void BufferLayout::AddArrayElementDescriptor(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t elementOffset,
        uint32_t elementRelativeOffset,
        uint32_t arrayAlignment,
        uint32_t stride,
        uint32_t arrayIndex
    ) {
        const BaseTypeInfo& typeInfo = GetBaseTypeInfo(fieldDef.baseType);

        FieldDescriptor desc;
        desc.name = fieldDef.name;
        desc.path = BuildPath(parentPath, fieldDef.name, true, arrayIndex);
        desc.baseType = fieldDef.baseType;
        desc.structTypeName.clear();
        desc.arraySize = fieldDef.arraySize;
        desc.arrayIndex = arrayIndex;
        desc.offset = elementOffset;
        desc.relativeOffset = elementRelativeOffset;
        desc.size = typeInfo.size;
        desc.alignment = arrayAlignment;
        desc.stride = stride;
        desc.parentPath = parentPath;
        desc.parentIndex = parentIndex;
        desc.isBaseType = true;
        desc.isArray = true;
        desc.isArrayElement = true;
        desc.accessMode = fieldDef.accessMode;

        m_allFields.push_back(desc);
    }

    void BufferLayout::AddStructArrayElementDescriptor(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t elementOffset,
        uint32_t elementRelativeOffset,
        uint32_t elementSize,
        uint32_t arrayAlignment,
        uint32_t stride,
        uint32_t arrayIndex
    ) {
        const std::string elementPath = BuildPath(parentPath, fieldDef.name, true, arrayIndex);
        const int32_t structDescIndex = static_cast<int32_t>(m_allFields.size());

        FieldDescriptor structDesc;
        structDesc.name = fieldDef.name;
        structDesc.path = elementPath;
        structDesc.baseType = BaseType::Unknown;
        structDesc.structTypeName = fieldDef.structDef->GetName();
        structDesc.arraySize = fieldDef.arraySize;
        structDesc.arrayIndex = arrayIndex;
        structDesc.offset = elementOffset;
        structDesc.relativeOffset = elementRelativeOffset;
        structDesc.size = elementSize;
        structDesc.alignment = arrayAlignment;
        structDesc.stride = stride;
        structDesc.parentPath = parentPath;
        structDesc.parentIndex = parentIndex;
        structDesc.isBaseType = false;
        structDesc.isArray = true;
        structDesc.isArrayElement = true;
        structDesc.accessMode = fieldDef.accessMode;

        m_allFields.push_back(structDesc);

        AddNestedStructFields(fieldDef.structDef, elementPath, structDescIndex, elementOffset);
    }

    std::string BufferLayout::BuildPath(
        const std::string& parentPath,
        const std::string& fieldName,
        bool isArrayElement,
        uint32_t arrayIndex
    ) const {
        std::string path;

        if (!parentPath.empty()) {
            path = parentPath + ".";
        }

        path += fieldName;

        if (isArrayElement) {
            path += "[" + std::to_string(arrayIndex) + "]";
        }

        return path;
    }

    // ============================================================================
    // MAP BUILDING
    // ============================================================================

    void BufferLayout::BuildMaps() {
        m_pathToIndex.clear();
        m_topLevelIndices.clear();

        m_pathToIndex.reserve(m_allFields.size());

        for (size_t i = 0; i < m_allFields.size(); ++i) {
            const auto& field = m_allFields[i];

            m_pathToIndex[std::string_view(field.path)] = i;

            // Top-level: parent == -1 i nie jest elementem tablicy
            if (field.parentIndex == -1 && !field.isArrayElement) {
                m_topLevelIndices.push_back(i);
            }
        }
    }

    // ============================================================================
    // FIELD ACCESS
    // ============================================================================

    const FieldDescriptor* BufferLayout::FindField(const std::string& path) const {
        auto it = m_pathToIndex.find(std::string_view(path));
        if (it == m_pathToIndex.end()) {
            return nullptr;
        }
        return &m_allFields[it->second];
    }

    const FieldDescriptor* BufferLayout::GetFieldByIndex(size_t index) const {
        if (index >= m_allFields.size()) {
            return nullptr;
        }
        return &m_allFields[index];
    }

    // ============================================================================
    // DEBUG UTILITIES
    // ============================================================================

    std::string BufferLayout::GetTypeString(const FieldDescriptor& field) const {
        if (field.isBaseType) {
            return BaseTypeToString(field.baseType);
        }
        else {
            return field.structTypeName;
        }
    }

    void BufferLayout::AppendFieldDebugInfo(
        std::stringstream& ss,
        const FieldDescriptor& field,
        int indentLevel,
        bool showPadding,
        uint32_t* lastOffset
    ) const {
        std::string indent(indentLevel * 2, ' ');

        // Show padding before this field
        if (showPadding && lastOffset && *lastOffset < field.offset) {
            uint32_t paddingSize = field.offset - *lastOffset;
            ss << indent << "  [PADDING: " << paddingSize << " bytes]\n";
        }

        // Field header
        ss << indent << "• " << field.name;

        // Czy to cała tablica czy konkretny element?
        if (field.isArray && !field.isArrayElement) {
            ss << "[" << field.arraySize << "]";
        }
        else if (field.isArrayElement) {
            ss << "[" << field.arrayIndex << "]";
        }

        ss << ": " << GetTypeString(field);

        if (!field.isBaseType) {
            ss << " (struct)";
        }

        if (field.isArray && !field.isArrayElement) {
            ss << " (array container)";
        }

        ss << "\n" << indent << "    abs_offset=" << std::setw(4) << field.offset
            << "  rel_offset=" << std::setw(4) << field.relativeOffset
            << "  size=" << std::setw(4) << field.size
            << "  align=" << std::setw(2) << field.alignment;

        if (field.stride > 0) {
            ss << "  stride=" << field.stride;
        }

        ss << "\n";

        // Recursively show nested fields for structures (nie dla array container)
        if (!field.isBaseType && field.isArrayElement) {
            std::vector<size_t> childIndices;
            for (size_t i = 0; i < m_allFields.size(); ++i) {
                const auto& childField = m_allFields[i];
                if (childField.parentPath == field.path) {
                    childIndices.push_back(i);
                }
            }

            std::sort(childIndices.begin(), childIndices.end(),
                [this](size_t a, size_t b) {
                    return m_allFields[a].offset < m_allFields[b].offset;
                });

            uint32_t nestedLastOffset = field.offset;

            for (size_t childIdx : childIndices) {
                const auto& childField = m_allFields[childIdx];
                AppendFieldDebugInfo(ss, childField, indentLevel + 1, showPadding, &nestedLastOffset);
            }

            if (showPadding) {
                uint32_t structEnd = field.offset + field.size;
                if (nestedLastOffset < structEnd) {
                    uint32_t structPadding = structEnd - nestedLastOffset;
                    std::string nestedIndent((indentLevel + 1) * 2, ' ');
                    ss << nestedIndent << "  [STRUCT END PADDING: " << structPadding << " bytes]\n";
                }
            }
        }

        if (lastOffset) {
            // Dla array container, lastOffset to koniec całej tablicy
            *lastOffset = field.offset + field.size;
        }
    }

    std::string BufferLayout::DebugPrint(bool showPadding) const {
        std::stringstream ss;

        ss << "========================================================\n";
        ss << "BUFFER LAYOUT: " << m_structure->GetName() << "\n";
        ss << "========================================================\n";
        ss << "Standard:       "
            << (m_standard == LayoutStandard::Std140 ? "std140" :
                m_standard == LayoutStandard::Std430 ? "std430" : "packed") << "\n";
        ss << "Total Size:     " << m_totalSize << " bytes\n";
        ss << "Alignment:      " << m_alignment << " bytes\n";
        ss << "Field Count:    " << m_allFields.size() << "\n";
        ss << "-------------------------------------------------------\n\n";

        uint32_t lastOffset = 0;

        for (size_t i = 0; i < m_allFields.size(); ++i) {
            const auto& field = m_allFields[i];

            // Wyświetl tylko top-level (nie array elements, nie nested fields)
            if (field.parentIndex == -1 && !field.isArrayElement) {
                AppendFieldDebugInfo(ss, field, 0, showPadding, &lastOffset);

                // Jeśli to array container, pokaż jego elementy
                if (field.isArray && !field.isArrayElement) {
                    for (uint32_t arrIdx = 0; arrIdx < field.arraySize; ++arrIdx) {
                        for (size_t j = i + 1; j < m_allFields.size(); ++j) {
                            const auto& arrField = m_allFields[j];
                            if (arrField.name == field.name &&
                                arrField.parentPath == field.parentPath &&
                                arrField.isArrayElement &&
                                arrField.arrayIndex == arrIdx) {
                                AppendFieldDebugInfo(ss, arrField, 1, showPadding, &lastOffset);
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (showPadding && lastOffset < m_totalSize) {
            uint32_t finalPadding = m_totalSize - lastOffset;
            ss << "  [FINAL PADDING: " << finalPadding << " bytes]\n";
        }

        ss << "-------------------------------------------------------\n";
        ss << "Total: " << m_totalSize << " bytes (aligned to " << m_alignment << ")\n";
        ss << "========================================================\n";

        return ss.str();
    }

    void BufferLayout::DebugPrintToConsole(bool showPadding) const {
        std::cout << DebugPrint(showPadding);
    }

    BufferLayout::ValidationResult BufferLayout::Validate() const {
        ValidationResult result;
        result.isValid = true;

        for (const auto& field : m_allFields) {
            if (field.offset >= m_totalSize) {
                result.errors.push_back(
                    "Field '" + field.path + "' offset (" +
                    std::to_string(field.offset) + ") exceeds buffer size (" +
                    std::to_string(m_totalSize) + ")"
                );
                result.isValid = false;
            }
        }

        for (const auto& field : m_allFields) {
            if (field.offset % field.alignment != 0) {
                result.errors.push_back(
                    "Field '" + field.path + "' offset (" +
                    std::to_string(field.offset) + ") not aligned to " +
                    std::to_string(field.alignment) + " bytes"
                );
                result.isValid = false;
            }
        }

        if (m_totalSize % m_alignment != 0) {
            result.errors.push_back(
                "Total size (" + std::to_string(m_totalSize) +
                ") not aligned to buffer alignment (" +
                std::to_string(m_alignment) + ")"
            );
            result.isValid = false;
        }

        return result;
    }

    // ============================================================================
    // SERIALIZATION
    // ============================================================================

    json BufferLayout::ToJson() const {
        json j;
        j["structure"] = m_structure->ToJson();
        j["standard"] = static_cast<int>(m_standard);
        j["totalSize"] = m_totalSize;
        j["alignment"] = m_alignment;
        return j;
    }

    std::shared_ptr<BufferLayout> BufferLayout::FromJson(const json& j) {
        auto structure = StructureDefinition::FromJson(j.at("structure"));
        LayoutStandard standard = static_cast<LayoutStandard>(
            j.at("standard").get<int>()
            );
        return std::make_shared<BufferLayout>(structure, standard);
    }

} // namespace ShaderLib
