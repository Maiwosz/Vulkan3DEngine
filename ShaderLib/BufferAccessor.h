#pragma once
#include "ShaderLib.h"
#include "ShaderTypes.h"
#include "IBufferMapping.h"
#include <cstring>
#include <stdexcept>

namespace ShaderLib {

    // ============================================================================
    // BUFFER ACCESSOR - Base class for BufferReader and BufferWriter
    // ============================================================================

    class BufferAccessor {
    public:
        BufferAccessor() = default;
        virtual ~BufferAccessor() = default;

        // Check if a variable exists
        bool hasVariable(const std::string& variableName) const {
            return findVariable(variableName) != nullptr;
        }

        // Get variable info
        const BufferVariable* getVariableInfo(const std::string& variableName) const {
            return findVariable(variableName);
        }

        // Check if accessor is properly initialized
        bool isValid() const {
            return m_bufferObject != nullptr && m_data != nullptr;
        }

        // Get buffer size
        size_t getDataSize() const {
            return m_dataSize;
        }

        // Get access mode for specific variable
        BufferAccessMode getVariableAccessMode(const std::string& variableName) const {
            const BufferVariable* variable = findVariable(variableName);
            return variable ? variable->accessMode : BufferAccessMode::ReadOnly;
        }

        // Check specific variable capabilities
        bool canReadVariable(const std::string& variableName) const {
            const BufferVariable* variable = findVariable(variableName);
            return variable && !variable->IsWriteOnly();
        }

        bool canWriteVariable(const std::string& variableName) const {
            const BufferVariable* variable = findVariable(variableName);
            return variable && !variable->IsReadOnly();
        }

    protected:
        const BufferObject* m_bufferObject = nullptr;
        void* m_data = nullptr;
        size_t m_dataSize = 0;
        IBufferMapping* m_bufferMapping = nullptr;

        const BufferVariable* findVariable(const std::string& name) const {
            if (!m_bufferObject) {
                return nullptr;
            }

            for (const auto& variable : m_bufferObject->variables) {
                if (variable.name == name) {
                    return &variable;
                }
            }

            return nullptr;
        }

        // Bounds checking helper
        bool checkBounds(size_t offset, size_t size) const {
            return offset + size <= m_dataSize;
        }
    };

} // namespace ShaderLib