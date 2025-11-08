#pragma once
#include "ShaderTypes.h"
#include "FieldDescriptor.h"
#include <vector>
#include <string>

namespace ShaderLib {

    class BufferObjectInstance;

    // ============================================================================
    // FIELD PROXY - Wygodny dostęp do pól przez routing w layoutcie
    // ============================================================================

    class FieldProxy {
    public:
        FieldProxy(BufferObjectInstance* instance, const FieldDescriptor* descriptor);

        // ========================================================================
        // VALUE ACCESS - dla base types
        // ========================================================================

        template<typename T>
        operator T() const;

        template<typename T>
        FieldProxy& operator=(const T& value);
        FieldProxy& operator=(const BaseTypeValue& value);

        // ========================================================================
        // NESTED STRUCTURE/ARRAY ACCESS
        // ========================================================================

        FieldProxy operator[](const std::string& childName) const;
        FieldProxy operator[](const char* childName) const;
        FieldProxy operator[](size_t index) const;

        // ========================================================================
        // METADATA ACCESS
        // ========================================================================

        const std::string& GetPath() const { return m_descriptor->path; }
        const std::string& GetName() const { return m_descriptor->name; }
        uint32_t GetOffset() const { return m_descriptor->offset; }
        uint32_t GetSize() const { return m_descriptor->size; }
        uint32_t GetAlignment() const { return m_descriptor->alignment; }
        BaseType GetBaseType() const { return m_descriptor->baseType; }
        bool IsBaseType() const { return m_descriptor->isBaseType; }
        bool IsArray() const { return m_descriptor->isArray; }
        bool IsArrayElement() const { return m_descriptor->isArrayElement; }
        uint32_t GetArraySize() const { return m_descriptor->arraySize; }
        uint32_t GetArrayIndex() const { return m_descriptor->arrayIndex; }

        // ========================================================================
        // RAW BUFFER ACCESS
        // ========================================================================

        uint8_t* GetRawPointer();
        const uint8_t* GetRawPointer() const;

    private:
        BufferObjectInstance* m_instance;
        const FieldDescriptor* m_descriptor;
    };

    // ============================================================================
    // TEMPLATE IMPLEMENTATIONS
    // ============================================================================

    template<typename T>
    inline FieldProxy::operator T() const {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        if (!m_descriptor->isBaseType) {
            throw std::runtime_error(
                "Cannot read structure field as value: " + m_descriptor->path
            );
        }

        if (m_descriptor->baseType != GetBaseTypeOf<T>()) {
            throw std::runtime_error("Type mismatch for field: " + m_descriptor->path);
        }

        T value;
        std::memcpy(&value, GetRawPointer(), sizeof(T));
        return value;
    }

    template<typename T>
    inline FieldProxy& FieldProxy::operator=(const T& value) {
        static_assert(IsBaseTypeSupported<T>(), "Type not supported");

        if (!m_descriptor->isBaseType) {
            throw std::runtime_error(
                "Cannot write structure field as value: " + m_descriptor->path
            );
        }

        if (m_descriptor->baseType != GetBaseTypeOf<T>()) {
            throw std::runtime_error("Type mismatch for field: " + m_descriptor->path);
        }

        std::memcpy(GetRawPointer(), &value, sizeof(T));
        return *this;
    }

} // namespace ShaderLib
