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
                    // std140: array elements aligned to max(alignment, 16)
                    uint32_t arrayAlignment = std::max(fieldAlignment, 16u);
                    stride = AlignTo(fieldSize, arrayAlignment);
                    fieldAlignment = arrayAlignment;
                }
                else {
                    // std430: array elements aligned to their natural alignment
                    stride = AlignTo(fieldSize, fieldAlignment);
                }

                // Align field start
                currentOffset = AlignTo(currentOffset, fieldAlignment);

                // Total array size
                uint32_t arraySize = field.arraySize * stride;
                currentOffset += arraySize;

                maxAlignment = std::max(maxAlignment, fieldAlignment);
            }
            else if (field.isBaseType()) {
                // Simple base type field
                const BaseTypeInfo& typeInfo = GetBaseTypeInfo(field.baseType);
                fieldAlignment = typeInfo.GetAlignment(standard);
                fieldSize = typeInfo.size;

                currentOffset = AlignTo(currentOffset, fieldAlignment);
                currentOffset += fieldSize;

                maxAlignment = std::max(maxAlignment, fieldAlignment);
            }
            else {
                // Nested struct field (not array)
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
            // std140: struct alignment rounded up to multiple of 16
            maxAlignment = AlignTo(maxAlignment, 16u);
        }
        // std430: struct alignment = max field alignment (no rounding)

        // Struct size must be multiple of its alignment
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

        // Process each top-level field
        for (const auto& fieldDef : fields) {
            currentOffset = ProcessField(
                fieldDef,
                "",           // parent path
                -1,           // parent index
                currentOffset
            );
        }

        // Buffer-level alignment rules (different from struct!)
        if (m_standard == LayoutStandard::Std140) {
            // std140: BUFFER (not struct) has minimum 16 byte alignment
            m_alignment = std::max(m_alignment, 16u);
        }
        // std430: buffer alignment = max field alignment

        // Total size must be multiple of alignment
        m_totalSize = AlignTo(currentOffset, m_alignment);
    }

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
        uint32_t currentOffset  // ABSOLUTNY offset
    ) {
        const BaseTypeInfo& typeInfo = GetBaseTypeInfo(fieldDef.baseType);
        const uint32_t alignment = typeInfo.GetAlignment(m_standard);

        // Align offset
        currentOffset = AlignTo(currentOffset, alignment);

        // Oblicz relative offset (dla top-level będzie równy absolutnemu)
        uint32_t relativeOffset = currentOffset;
        if (parentIndex >= 0) {
            relativeOffset = currentOffset - m_allFields[parentIndex].offset;
        }

        // Create descriptor
        FieldDescriptor desc;
        desc.name = fieldDef.name;
        desc.path = parentPath.empty() ? fieldDef.name : parentPath + "." + fieldDef.name;
        desc.baseType = fieldDef.baseType;
        desc.structTypeName.clear();
        desc.arraySize = 0;
        desc.arrayIndex = 0;
        desc.offset = currentOffset;           // ABSOLUTNY
        desc.relativeOffset = relativeOffset;  // WZGLĘDNY
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

        // Update buffer alignment
        m_alignment = std::max(m_alignment, alignment);

        return currentOffset + typeInfo.size;
    }

    uint32_t BufferLayout::ProcessArray(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t currentOffset  // ABSOLUTNY
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
            // Nested struct - use helper to compute layout
            StructLayoutInfo structInfo = ComputeStructLayout(fieldDef.structDef, m_standard);
            elementSize = structInfo.size;
            elementAlignment = structInfo.alignment;
        }

        // Array stride depends on layout standard
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

        // Align start of array
        currentOffset = AlignTo(currentOffset, arrayAlignment);
        const uint32_t arrayStart = currentOffset;

        // Oblicz relative offset (dla top-level będzie równy absolutnemu)
        uint32_t baseRelativeOffset = arrayStart;
        if (parentIndex >= 0) {
            baseRelativeOffset = arrayStart - m_allFields[parentIndex].offset;
        }

        // Process each array element
        for (uint32_t i = 0; i < fieldDef.arraySize; ++i) {
            const uint32_t elementOffset = arrayStart + (i * stride);
            const uint32_t elementRelativeOffset = baseRelativeOffset + (i * stride);

            if (fieldDef.isBaseType()) {
                // Base type array element
                const BaseTypeInfo& typeInfo = GetBaseTypeInfo(fieldDef.baseType);

                FieldDescriptor desc;
                desc.name = fieldDef.name;
                desc.path = parentPath.empty()
                    ? fieldDef.name + "[" + std::to_string(i) + "]"
                    : parentPath + "." + fieldDef.name + "[" + std::to_string(i) + "]";
                desc.baseType = fieldDef.baseType;
                desc.structTypeName.clear();
                desc.arraySize = fieldDef.arraySize;
                desc.arrayIndex = i;
                desc.offset = elementOffset;                    // ABSOLUTNY
                desc.relativeOffset = elementRelativeOffset;    // WZGLĘDNY
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
            else {
                // Struct array element
                const std::string elementPath = parentPath.empty()
                    ? fieldDef.name + "[" + std::to_string(i) + "]"
                    : parentPath + "." + fieldDef.name + "[" + std::to_string(i) + "]";

                const int32_t structDescIndex = static_cast<int32_t>(m_allFields.size());

                // Add struct descriptor
                FieldDescriptor structDesc;
                structDesc.name = fieldDef.name;
                structDesc.path = elementPath;
                structDesc.baseType = BaseType::Unknown;
                structDesc.structTypeName = fieldDef.structDef->GetName();
                structDesc.arraySize = fieldDef.arraySize;
                structDesc.arrayIndex = i;
                structDesc.offset = elementOffset;                  // ABSOLUTNY
                structDesc.relativeOffset = elementRelativeOffset;  // WZGLĘDNY
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

                // Recursively add nested struct fields
                AddNestedStructFields(
                    fieldDef.structDef,
                    elementPath,
                    structDescIndex,
                    elementOffset  // ABSOLUTNY offset początku struktury
                );
            }
        }

        // Update buffer alignment
        m_alignment = std::max(m_alignment, arrayAlignment);

        return arrayStart + (fieldDef.arraySize * stride);
    }

    uint32_t BufferLayout::ProcessStruct(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t currentOffset  // ABSOLUTNY
    ) {
        // Compute struct layout using helper
        StructLayoutInfo structInfo = ComputeStructLayout(fieldDef.structDef, m_standard);

        const uint32_t structSize = structInfo.size;
        const uint32_t structAlignment = structInfo.alignment;

        // Align to struct alignment
        currentOffset = AlignTo(currentOffset, structAlignment);

        // Oblicz relative offset (dla top-level będzie równy absolutnemu)
        uint32_t relativeOffset = currentOffset;
        if (parentIndex >= 0) {
            relativeOffset = currentOffset - m_allFields[parentIndex].offset;
        }

        const std::string structPath = parentPath.empty()
            ? fieldDef.name
            : parentPath + "." + fieldDef.name;

        const int32_t structDescIndex = static_cast<int32_t>(m_allFields.size());

        // Add struct descriptor
        FieldDescriptor structDesc;
        structDesc.name = fieldDef.name;
        structDesc.path = structPath;
        structDesc.baseType = BaseType::Unknown;
        structDesc.structTypeName = fieldDef.structDef->GetName();
        structDesc.arraySize = 0;
        structDesc.arrayIndex = 0;
        structDesc.offset = currentOffset;           // ABSOLUTNY
        structDesc.relativeOffset = relativeOffset;  // WZGLĘDNY
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
        AddNestedStructFields(
            fieldDef.structDef,
            structPath,
            structDescIndex,
            currentOffset  // ABSOLUTNY offset początku struktury
        );

        // Update buffer alignment
        m_alignment = std::max(m_alignment, structAlignment);

        return currentOffset + structSize;
    }

    void BufferLayout::AddNestedStructFields(
        std::shared_ptr<const StructureDefinition> structDef,
        const std::string& structPath,
        int32_t structParentIndex,
        uint32_t baseOffset  // ABSOLUTNY offset początku struktury
    ) {
        // Teraz po prostu przekazujemy absolutny offset do ProcessNestedField
        // i nie musimy już żonglować lokalnym offsetem!
        uint32_t currentOffset = baseOffset;

        for (const auto& field : structDef->GetFields()) {
            currentOffset = ProcessNestedField(
                field,
                structPath,
                structParentIndex,
                currentOffset  // Zawsze absolutny offset
            );
        }
    }

    uint32_t BufferLayout::ProcessNestedField(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t currentOffset
    ) {
        if (fieldDef.isArray()) {
            return ProcessNestedArray(fieldDef, parentPath, parentIndex, currentOffset);
        }
        else if (fieldDef.isBaseType()) {
            return ProcessNestedBaseType(fieldDef, parentPath, parentIndex, currentOffset);
        }
        else {
            return ProcessNestedStruct(fieldDef, parentPath, parentIndex, currentOffset);
        }
    }

    uint32_t BufferLayout::ProcessNestedBaseType(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t currentOffset  // ABSOLUTNY
    ) {
        const BaseTypeInfo& typeInfo = GetBaseTypeInfo(fieldDef.baseType);
        const uint32_t alignment = typeInfo.GetAlignment(m_standard);

        currentOffset = AlignTo(currentOffset, alignment);

        // Oblicz relative offset
        uint32_t relativeOffset = currentOffset - m_allFields[parentIndex].offset;

        FieldDescriptor desc;
        desc.name = fieldDef.name;
        desc.path = parentPath + "." + fieldDef.name;
        desc.baseType = fieldDef.baseType;
        desc.structTypeName.clear();
        desc.arraySize = 0;
        desc.arrayIndex = 0;
        desc.offset = currentOffset;           // ABSOLUTNY
        desc.relativeOffset = relativeOffset;  // WZGLĘDNY
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

        return currentOffset + typeInfo.size;
    }

    uint32_t BufferLayout::ProcessNestedArray(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t currentOffset  // ABSOLUTNY
    ) {
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

        // Oblicz base relative offset dla pierwszego elementu
        const uint32_t baseRelativeOffset = arrayStart - m_allFields[parentIndex].offset;

        for (uint32_t i = 0; i < fieldDef.arraySize; ++i) {
            const uint32_t elementOffset = arrayStart + (i * stride);
            const uint32_t elementRelativeOffset = baseRelativeOffset + (i * stride);

            if (fieldDef.isBaseType()) {
                const BaseTypeInfo& typeInfo = GetBaseTypeInfo(fieldDef.baseType);

                FieldDescriptor desc;
                desc.name = fieldDef.name;
                desc.path = parentPath + "." + fieldDef.name + "[" + std::to_string(i) + "]";
                desc.baseType = fieldDef.baseType;
                desc.structTypeName.clear();
                desc.arraySize = fieldDef.arraySize;
                desc.arrayIndex = i;
                desc.offset = elementOffset;                    // ABSOLUTNY
                desc.relativeOffset = elementRelativeOffset;    // WZGLĘDNY
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
            else {
                const std::string elementPath = parentPath + "." + fieldDef.name +
                    "[" + std::to_string(i) + "]";
                const int32_t structDescIndex = static_cast<int32_t>(m_allFields.size());

                FieldDescriptor structDesc;
                structDesc.name = fieldDef.name;
                structDesc.path = elementPath;
                structDesc.baseType = BaseType::Unknown;
                structDesc.structTypeName = fieldDef.structDef->GetName();
                structDesc.arraySize = fieldDef.arraySize;
                structDesc.arrayIndex = i;
                structDesc.offset = elementOffset;                  // ABSOLUTNY
                structDesc.relativeOffset = elementRelativeOffset;  // WZGLĘDNY
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

                AddNestedStructFields(
                    fieldDef.structDef,
                    elementPath,
                    structDescIndex,
                    elementOffset  // ABSOLUTNY offset początku struktury
                );
            }
        }

        return arrayStart + (fieldDef.arraySize * stride);
    }

    uint32_t BufferLayout::ProcessNestedStruct(
        const StructureDefinition::FieldDef& fieldDef,
        const std::string& parentPath,
        int32_t parentIndex,
        uint32_t currentOffset  // ABSOLUTNY
    ) {
        StructLayoutInfo structInfo = ComputeStructLayout(fieldDef.structDef, m_standard);

        const uint32_t structSize = structInfo.size;
        const uint32_t structAlignment = structInfo.alignment;

        currentOffset = AlignTo(currentOffset, structAlignment);

        // Oblicz relative offset
        const uint32_t relativeOffset = currentOffset - m_allFields[parentIndex].offset;

        const std::string structPath = parentPath + "." + fieldDef.name;
        const int32_t structDescIndex = static_cast<int32_t>(m_allFields.size());

        FieldDescriptor structDesc;
        structDesc.name = fieldDef.name;
        structDesc.path = structPath;
        structDesc.baseType = BaseType::Unknown;
        structDesc.structTypeName = fieldDef.structDef->GetName();
        structDesc.arraySize = 0;
        structDesc.arrayIndex = 0;
        structDesc.offset = currentOffset;           // ABSOLUTNY
        structDesc.relativeOffset = relativeOffset;  // WZGLĘDNY
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

        AddNestedStructFields(
            fieldDef.structDef,
            structPath,
            structDescIndex,
            currentOffset  // ABSOLUTNY offset początku struktury
        );

        return currentOffset + structSize;
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

            if (field.parentIndex == -1 && field.arrayIndex == 0) {
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

        // Show padding before this field (only if we're tracking offsets at this level)
        if (showPadding && lastOffset && *lastOffset < field.offset) {
            uint32_t paddingSize = field.offset - *lastOffset;
            ss << indent << "  [PADDING: " << paddingSize << " bytes]\n";
        }

        // Field header
        ss << indent << "• " << field.name;

        if (field.isArrayElement) {
            ss << "[" << field.arrayIndex << "]";
        }

        ss << ": " << GetTypeString(field);

        // Show struct keyword for structure types
        if (!field.isBaseType) {
            ss << " (struct)";
        }

        ss << indent << "    abs_offset=" << std::setw(4) << field.offset
            << "  rel_offset=" << std::setw(4) << field.relativeOffset
            << "  size=" << std::setw(4) << field.size
            << "  align=" << std::setw(2) << field.alignment;

        if (field.stride > 0) {
            ss << "  stride=" << field.stride;
        }

        ss << "\n";

        // If this is a structure, recursively show its nested fields
        if (!field.isBaseType) {
            // Find all direct children of this struct and sort them by offset
            // KRYTYCZNE: Dzieci nie są w kolejności w m_allFields,
            // musimy je posortować po offsetach
            std::vector<size_t> childIndices;
            for (size_t i = 0; i < m_allFields.size(); ++i) {
                const auto& childField = m_allFields[i];
                if (childField.parentPath == field.path) {
                    childIndices.push_back(i);
                }
            }

            // Sort children by offset
            std::sort(childIndices.begin(), childIndices.end(),
                [this](size_t a, size_t b) {
                    return m_allFields[a].offset < m_allFields[b].offset;
                });

            // Now print children in order with correct padding calculation
            uint32_t nestedLastOffset = field.offset;

            for (size_t childIdx : childIndices) {
                const auto& childField = m_allFields[childIdx];
                AppendFieldDebugInfo(ss, childField, indentLevel + 1, showPadding, &nestedLastOffset);
            }

            // Show padding at the end of struct if any
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
            *lastOffset = field.offset + field.size;
        }
    }

    std::string BufferLayout::DebugPrint(bool showPadding) const {
        std::stringstream ss;

        ss << "═══════════════════════════════════════════════════════\n";
        ss << "BUFFER LAYOUT: " << m_structure->GetName() << "\n";
        ss << "═══════════════════════════════════════════════════════\n";
        ss << "Standard:       "
            << (m_standard == LayoutStandard::Std140 ? "std140" :
                m_standard == LayoutStandard::Std430 ? "std430" : "packed") << "\n";
        ss << "Total Size:     " << m_totalSize << " bytes\n";
        ss << "Alignment:      " << m_alignment << " bytes\n";
        ss << "Field Count:    " << m_allFields.size() << "\n";
        ss << "───────────────────────────────────────────────────────\n\n";

        uint32_t lastOffset = 0;

        // Only print top-level fields - nested ones will be printed recursively
        for (size_t i = 0; i < m_allFields.size(); ++i) {
            const auto& field = m_allFields[i];
            if (field.parentIndex == -1 && (!field.isArrayElement || field.arrayIndex == 0)) {
                AppendFieldDebugInfo(ss, field, 0, showPadding, &lastOffset);

                // For arrays, show all elements
                if (field.isArray && field.arrayIndex == 0) {
                    for (uint32_t arrIdx = 1; arrIdx < field.arraySize; ++arrIdx) {
                        // Find next array element
                        for (size_t j = i + 1; j < m_allFields.size(); ++j) {
                            const auto& arrField = m_allFields[j];
                            if (arrField.name == field.name &&
                                arrField.parentPath == field.parentPath &&
                                arrField.arrayIndex == arrIdx) {
                                AppendFieldDebugInfo(ss, arrField, 0, showPadding, &lastOffset);
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

        ss << "───────────────────────────────────────────────────────\n";
        ss << "Total: " << m_totalSize << " bytes (aligned to " << m_alignment << ")\n";
        ss << "═══════════════════════════════════════════════════════\n";

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
