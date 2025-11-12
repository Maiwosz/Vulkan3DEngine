#pragma once
#include "ShaderTypes.h"
#include <string>

namespace ShaderLib {

    // ============================================================================
    // FIELD DESCRIPTOR - Metadane pojedynczego pola w płaskim layoutcie
    // 
    // WAŻNE: Dla tablic przechowujemy JEDEN deskryptor dla całej tablicy.
    // Dostęp do elementów odbywa się przez offset + (index * stride).
    // ============================================================================

    struct FieldDescriptor {
        // Identyfikacja
        std::string name;              // Nazwa pola (bez ścieżki)
        std::string path;              // Pełna ścieżka: "transform.position" lub "colors" (bez indeksu!)

        // Typ
        BaseType baseType;             // Dla base types (Unknown dla struktur)
        std::string structTypeName;    // Dla zagnieżdżonych struktur (pusta dla base types)

        // Array info
        uint32_t arraySize;            // 0 = nie array, >0 = liczba elementów w tablicy

        // Layout info (precomputowany dla wybranego standardu)
        uint32_t offset;               // Absolute offset początku tablicy/pola w buforze bajtów
        uint32_t relativeOffset;       // Offset względem rodzica (0 dla top-level)
        uint32_t size;                 // Rozmiar pojedynczego elementu (dla array) lub całego pola
        uint32_t totalSize;            // Całkowity rozmiar w bajtach (dla array: arraySize * stride)
        uint32_t alignment;            // Wymagane wyrównanie
        uint32_t stride;               // Stride dla array (0 jeśli nie array)

        // Hierarchia (przez indeksy)
        std::string parentPath;        // Ścieżka rodzica (pusta dla top-level)
        int32_t parentIndex;           // Indeks rodzica w tablicy deskryptorów (-1 dla top-level)

        // Flags
        bool isBaseType;               // true = base type, false = struktura
        bool isArray;                  // true jeśli to pole array
        BufferAccessMode accessMode;

        // Helpers
        bool IsStructure() const { return !isBaseType; }
        bool HasParent() const { return parentIndex >= 0; }

        // Oblicz offset dla konkretnego elementu tablicy
        uint32_t GetElementOffset(uint32_t index) const {
            if (!isArray) {
                throw std::runtime_error("Field is not an array");
            }
            if (index >= arraySize) {
                throw std::runtime_error("Array index out of bounds");
            }
            return offset + (index * stride);
        }

        FieldDescriptor()
            : baseType(BaseType::Unknown)
            , structTypeName()
            , arraySize(0)
            , offset(0)
            , relativeOffset(0)
            , size(0)
            , totalSize(0)
            , alignment(0)
            , stride(0)
            , parentIndex(-1)
            , isBaseType(true)
            , isArray(false)
            , accessMode(BufferAccessMode::ReadWrite)
        {
        }
    };

} // namespace ShaderLib
