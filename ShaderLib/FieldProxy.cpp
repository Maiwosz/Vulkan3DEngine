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
        const FieldDescriptor* descriptor
    )
        : m_instance(instance)
        , m_descriptor(descriptor)
    {
        if (!instance) {
            throw std::runtime_error("Instance cannot be null");
        }
        if (!descriptor) {
            throw std::runtime_error("Descriptor cannot be null");
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

        std::string childPath = m_descriptor->path + "." + childName;

        const FieldDescriptor* childDesc = m_instance->GetDefinition()->FindField(childPath);
        if (!childDesc) {
            throw std::runtime_error("Child field not found: " + childPath);
        }

        return FieldProxy(m_instance, childDesc);
    }

    FieldProxy FieldProxy::operator[](const char* childName) const {
        return operator[](std::string(childName));
    }

    FieldProxy FieldProxy::operator[](size_t index) const {
        if (!m_descriptor->isArray && !m_descriptor->isArrayElement) {
            throw std::runtime_error("Field is not an array: " + m_descriptor->path);
        }

        std::string basePath = m_descriptor->path;
        size_t bracketPos = basePath.find('[');
        if (bracketPos != std::string::npos) {
            basePath = basePath.substr(0, bracketPos);
        }

        std::string elementPath = basePath + "[" + std::to_string(index) + "]";

        const FieldDescriptor* elementDesc =
            m_instance->GetDefinition()->FindField(elementPath);
        if (!elementDesc) {
            throw std::runtime_error("Array element not found: " + elementPath);
        }

        return FieldProxy(m_instance, elementDesc);
    }

    // ========================================================================
    // RAW BUFFER ACCESS
    // ========================================================================

    uint8_t* FieldProxy::GetRawPointer() {
        return m_instance->GetRawBuffer() + m_descriptor->offset;
    }

    const uint8_t* FieldProxy::GetRawPointer() const {
        return m_instance->GetRawBuffer() + m_descriptor->offset;
    }

} // namespace ShaderLib
