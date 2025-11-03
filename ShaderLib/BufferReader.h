#pragma once
#include "BufferAccessor.h"
#include "ValueSerialization.h"
#include "ShaderArrayInstance.h"
#include "ShaderStructInstance.h"
#include <cstring>

namespace ShaderLib {

    class BufferReader : public BufferAccessor {
    public:
        BufferReader() = default;

        ~BufferReader() {
            // Automatyczne odmapowanie przy destrukcji
            if (m_bufferMapping && m_bufferMapping->isMapped()) {
                m_bufferMapping->unmap();
            }
        }

        BufferReader(const BufferReader&) = delete;
        BufferReader& operator=(const BufferReader&) = delete;

        BufferReader(BufferReader&& other) noexcept
            : BufferAccessor() {
            m_bufferObject = other.m_bufferObject;
            m_data = other.m_data;
            m_dataSize = other.m_dataSize;
            m_bufferMapping = other.m_bufferMapping;

            other.m_bufferObject = nullptr;
            other.m_data = nullptr;
            other.m_dataSize = 0;
            other.m_bufferMapping = nullptr;
        }

        BufferReader& operator=(BufferReader&& other) noexcept {
            if (this != &other) {
                // Odmapuj obecny bufor
                if (m_bufferMapping && m_bufferMapping->isMapped()) {
                    m_bufferMapping->unmap();
                }

                m_bufferObject = other.m_bufferObject;
                m_data = other.m_data;
                m_dataSize = other.m_dataSize;
                m_bufferMapping = other.m_bufferMapping;

                other.m_bufferObject = nullptr;
                other.m_data = nullptr;
                other.m_dataSize = 0;
                other.m_bufferMapping = nullptr;
            }
            return *this;
        }

        void setBuffer(const BufferObject* bufferObject, const void* data, size_t dataSize) {
            m_bufferObject = bufferObject;
            m_data = const_cast<void*>(data);
            m_dataSize = dataSize;
            m_bufferMapping = nullptr;
        }

        bool setBufferWithMapping(const BufferObject* bufferObject,
            IBufferMapping* bufferMapping,
            size_t dataSize) {
            if (!bufferMapping) {
                return false;
            }

            m_bufferObject = bufferObject;
            m_bufferMapping = bufferMapping;
            m_dataSize = dataSize;

            // Automatyczne mapowanie
            m_data = m_bufferMapping->map();
            return m_data != nullptr;
        }

        bool readRaw(void* outData, size_t size, size_t offset = 0) const {
            if (!isValid() || !outData) {
                return false;
            }

            if (!checkBounds(offset, size)) {
                return false;
            }

            const uint8_t* src = static_cast<const uint8_t*>(m_data) + offset;
            std::memcpy(outData, src, size);
            return true;
        }

        // Read a variable by name into a BufferValue
        bool read(const std::string& variableName, BufferValue& outValue) const {
            if (!isValid()) {
                return false;
            }

            const BufferVariable* variable = findVariable(variableName);
            if (!variable) {
                return false;
            }

            if (!checkBounds(variable->offset, variable->size)) {
                return false;
            }

            const uint8_t* src = static_cast<const uint8_t*>(m_data) + variable->offset;

            try {
                // Handle composite types
                if (variable->IsComposite()) {
                    auto instance = variable->composite->CreateInstance();
                    if (!instance) {
                        return false;
                    }

                    if (!instance->ReadFromBuffer(src)) {
                        return false;
                    }

                    // Convert to appropriate derived type for BufferValue
                    if (instance->IsStruct()) {
                        outValue = std::static_pointer_cast<ShaderStructInstance>(instance);
                    }
                    else if (instance->IsArray()) {
                        outValue = std::static_pointer_cast<ShaderArrayInstance>(instance);
                    }
                    else {
                        return false;
                    }

                    return true;
                }

                // Handle base types
                const BaseTypeInfo& typeInfo = GetBaseTypeInfo(variable->baseType);
                if (!typeInfo.IsValid()) {
                    return false;
                }

                outValue = ReadBaseTypeFromBuffer(variable->baseType, src);
                return true;
            }
            catch (...) {
                return false;
            }
        }

        // Read a variable by name with type checking (base types)
        template<typename T>
        bool read(const std::string& variableName, T& outValue) const {
            static_assert(BaseTypeTraits<T>::supported, "Type not supported by BufferReader");

            if (!isValid()) {
                return false;
            }

            const BufferVariable* variable = findVariable(variableName);
            if (!variable) {
                return false;
            }

            if (variable->baseType != BaseTypeTraits<T>::type) {
                return false;
            }

            if (!checkBounds(variable->offset, sizeof(T))) {
                return false;
            }

            const uint8_t* src = static_cast<const uint8_t*>(m_data) + variable->offset;

            // Special handling for bool (stored as uint32_t in shaders)
            if constexpr (std::is_same_v<T, bool>) {
                uint32_t shaderBool;
                std::memcpy(&shaderBool, src, sizeof(uint32_t));
                outValue = (shaderBool != 0);
            }
            else {
                std::memcpy(&outValue, src, sizeof(T));
            }

            return true;
        }

        bool readComposite(const std::string& variableName,
            std::shared_ptr<CompositeTypeInstance>& outInstance) const {
            if (!isValid()) {
                return false;
            }

            const BufferVariable* variable = findVariable(variableName);
            if (!variable) {
                return false;
            }

            if (!variable->IsComposite()) {
                return false;
            }

            if (!checkBounds(variable->offset, variable->size)) {
                return false;
            }

            const uint8_t* src = static_cast<const uint8_t*>(m_data) + variable->offset;

            try {
                auto instance = variable->composite->CreateInstance();
                if (!instance) {
                    return false;
                }

                if (!instance->ReadFromBuffer(src)) {
                    return false;
                }

                outInstance = instance;
                return true;
            }
            catch (...) {
                return false;
            }
        }

        // Read ShaderStruct specifically
        template<typename = void>
        bool readStruct(const std::string& variableName,
            std::shared_ptr<ShaderStructInstance>& outStruct) const {
            std::shared_ptr<CompositeTypeInstance> instance;
            if (!readComposite(variableName, instance)) {
                return false;
            }

            if (!instance->IsStruct()) {
                return false;
            }

            outStruct = std::static_pointer_cast<ShaderStructInstance>(instance);
            return true;
        }

        // Read ShaderArray specifically
        template<typename = void>
        bool readArray(const std::string& variableName,
            std::shared_ptr<ShaderArrayInstance>& outArray) const {
            std::shared_ptr<CompositeTypeInstance> instance;
            if (!readComposite(variableName, instance)) {
                return false;
            }

            if (!instance->IsArray()) {
                return false;
            }

            outArray = std::static_pointer_cast<ShaderArrayInstance>(instance);
            return true;
        }

        // Helper: Get the type category of a variable
        ShaderTypeCategory getVariableCategory(const std::string& variableName) const {
            const BufferVariable* variable = findVariable(variableName);
            if (!variable) {
                return ShaderTypeCategory::Unknown;
            }
            return variable->GetCategory();
        }
    };

} // namespace ShaderLib