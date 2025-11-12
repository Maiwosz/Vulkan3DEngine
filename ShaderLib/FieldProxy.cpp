#include "pch.h"
#include "FieldProxy.h"
#include "BufferObjectInstance.h"
#include <stdexcept>

namespace ShaderLib {

    // ============================================================================
    // FIELD PROXY IMPLEMENTATION
    // ============================================================================

    FieldProxy::FieldProxy(
        BufferObjectInstance* instance,
        const FieldDescriptor* descriptor,
        uint32_t arrayIndex
    )
        : m_instance(instance)
        , m_descriptor(descriptor)
        , m_arrayIndex(arrayIndex)
    {
        if (!instance) {
            throw std::runtime_error("Instance cannot be null");
        }
        if (!descriptor) {
            throw std::runtime_error("Descriptor cannot be null");
        }
        if (descriptor->isArray && arrayIndex >= descriptor->arraySize) {
            throw std::runtime_error(
                "Array index " + std::to_string(arrayIndex) +
                " out of bounds for array of size " +
                std::to_string(descriptor->arraySize)
            );
        }
    }

    // ========================================================================
    // NESTED ACCESS
    // ========================================================================

    FieldProxy& FieldProxy::operator=(const BaseTypeValue& value) {
        if (!m_descriptor->isBaseType) {
            throw std::runtime_error(
                "Cannot assign to structure field: " + m_descriptor->path
            );
        }

        std::visit([&](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            *this = val;
            }, value);

        return *this;
    }

    FieldProxy FieldProxy::operator[](const std::string& childName) const {
        if (m_descriptor->isBaseType) {
            throw std::runtime_error(
                "Cannot access child of base type field: " + m_descriptor->path
            );
        }

        // Buduj ścieżkę do child (bez indeksu tablicy w path!)
        std::string childPath = m_descriptor->path + "." + childName;

        const FieldDescriptor* childDesc = m_instance->GetDefinition()->FindField(childPath);
        if (!childDesc) {
            throw std::runtime_error("Child field not found: " + childPath);
        }

        // Dla tablicy struktur przekaż ten sam arrayIndex
        return FieldProxy(m_instance, childDesc, m_arrayIndex);
    }

    FieldProxy FieldProxy::operator[](const char* childName) const {
        return operator[](std::string(childName));
    }

    FieldProxy FieldProxy::operator[](size_t index) const {
        if (!m_descriptor->isArray) {
            throw std::runtime_error("Field is not an array: " + m_descriptor->path);
        }

        if (index >= m_descriptor->arraySize) {
            throw std::runtime_error(
                "Array index " + std::to_string(index) +
                " out of bounds for array of size " +
                std::to_string(m_descriptor->arraySize)
            );
        }

        // Zwróć nowy proxy z tym samym deskryptorem ale innym indeksem
        return FieldProxy(m_instance, m_descriptor, static_cast<uint32_t>(index));
    }

    // ========================================================================
    // METADATA ACCESS
    // ========================================================================

    uint32_t FieldProxy::GetOffset() const {
        if (m_descriptor->isArray) {
            return m_descriptor->GetElementOffset(m_arrayIndex);
        }
        return m_descriptor->offset;
    }

    // ========================================================================
    // RAW BUFFER ACCESS
    // ========================================================================

    uint8_t* FieldProxy::GetRawPointer() {
        return m_instance->GetRawBuffer() + GetOffset();
    }

    const uint8_t* FieldProxy::GetRawPointer() const {
        return m_instance->GetRawBuffer() + GetOffset();
    }

} // namespace ShaderLib
