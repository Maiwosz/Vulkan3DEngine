#pragma once
#include "ShaderTypes.h"
#include <string>

namespace ShaderLib {

    // ============================================================================
    // FIELD DESCRIPTOR - Metadane pojedynczego pola w płaskim layoucie
    // 
    // Używane przez BufferLayout do opisania każdego pola w spłaszczonej hierarchii.
    // NIE zawiera już shared_ptr do StructureDefinition - tylko nazwę typu.
    // ============================================================================

    struct FieldDescriptor {
        // Identyfikacja
        std::string name;              // Nazwa pola (bez ścieżki)
        std::string path;              // Pełna ścieżka: "transform.position" lub "colors[2]"

        // Typ
        BaseType baseType;             // Dla base types (Unknown dla struktur)
        std::string structTypeName;    // Dla zagnieżdżonych struktur (pusta dla base types)

        // Array info
        uint32_t arraySize;            // 0 = nie array, >0 = array size
        uint32_t arrayIndex;           // Indeks w array (0 dla non-array)

        // Layout info (precomputowany dla wybranego standardu)
        uint32_t offset;               // Absolute offset w buforze bajtów
        uint32_t relativeOffset;       // Offset względem rodzica (0 dla top-level)
        uint32_t size;                 // Rozmiar w bajtach
        uint32_t alignment;            // Wymagane wyrównanie
        uint32_t stride;               // Stride dla array (0 jeśli nie array)

        // Hierarchia (przez indeksy)
        std::string parentPath;        // Ścieżka rodzica (pusta dla top-level)
        int32_t parentIndex;           // Indeks rodzica w tablicy descriptorów (-1 dla top-level)

        // Flags
        bool isBaseType;               // true = base type, false = struktura
        bool isArray;                  // true jeśli to pole array
        bool isArrayElement;           // true jeśli to element array
        BufferAccessMode accessMode;

        // Helpers
        bool IsStructure() const { return !isBaseType; }
        bool HasParent() const { return parentIndex >= 0; }

        FieldDescriptor()
            : baseType(BaseType::Unknown)
            , structTypeName()
            , arraySize(0)
            , arrayIndex(0)
            , offset(0)
            , relativeOffset(0)
            , size(0)
            , alignment(0)
            , stride(0)
            , parentIndex(-1)
            , isBaseType(true)
            , isArray(false)
            , isArrayElement(false)
            , accessMode(BufferAccessMode::ReadWrite)
        {
        }
    };

} // namespace ShaderLib
