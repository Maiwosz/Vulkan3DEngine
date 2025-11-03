#pragma once
#include "BufferAccessor.h"
#include "ValueSerialization.h"
#include "ShaderArrayInstance.h"
#include "ShaderStructInstance.h"
#include <cstring>

namespace ShaderLib {

    class BufferWriter : public BufferAccessor {
    public:
        BufferWriter() = default;

        // Destruktor - automatyczne odmapowanie
        ~BufferWriter() {
            if (m_bufferMapping && m_bufferMapping->isMapped()) {
                m_bufferMapping->unmap();
            }
        }

        // Zablokuj kopiowanie (RAII)
        BufferWriter(const BufferWriter&) = delete;
        BufferWriter& operator=(const BufferWriter&) = delete;

        // Pozwól na przenoszenie
        BufferWriter(BufferWriter&& other) noexcept {
            m_bufferObject = other.m_bufferObject;
            m_data = other.m_data;
            m_dataSize = other.m_dataSize;
            m_bufferMapping = other.m_bufferMapping;

            other.m_bufferObject = nullptr;
            other.m_data = nullptr;
            other.m_dataSize = 0;
            other.m_bufferMapping = nullptr;
        }

        BufferWriter& operator=(BufferWriter&& other) noexcept {
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

        // ręczne podanie wskaźnika (np. dla testów)
        void setBuffer(const BufferObject* bufferObject, void* data, size_t dataSize) {
            m_bufferObject = bufferObject;
            m_data = data;
            m_dataSize = dataSize;
            m_bufferMapping = nullptr;
        }

        // automatyczne mapowanie z RAII
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

        bool writeRaw(const void* data, size_t size, size_t offset = 0) {
            if (!isValid() || !data) {
                return false;
            }

            if (!checkBounds(offset, size)) {
                return false;
            }

            uint8_t* dst = static_cast<uint8_t*>(m_data) + offset;
            std::memcpy(dst, data, size);
            return true;
        }

        // Write a variable by name from a BufferValue
        bool write(const std::string& variableName, const BufferValue& value) {
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

            uint8_t* dst = static_cast<uint8_t*>(m_data) + variable->offset;

            try {
                // Handle composite types
                if (variable->IsComposite()) {
                    std::shared_ptr<CompositeTypeInstance> instance = nullptr;

                    if (auto structPtr = std::get_if<std::shared_ptr<ShaderStructInstance>>(&value)) {
                        instance = std::static_pointer_cast<CompositeTypeInstance>(*structPtr);
                    }
                    else if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArrayInstance>>(&value)) {
                        instance = std::static_pointer_cast<CompositeTypeInstance>(*arrayPtr);
                    }

                    if (!instance) {
                        return false;
                    }

                    if (instance->GetDefinition()->GetTypeName() !=
                        variable->composite->GetTypeName()) {
                        return false;
                    }

                    return instance->WriteToBuffer(dst);
                }

                // Handle base types
                BaseType variantType = GetBaseTypeFromVariant(value);
                if (variantType != variable->baseType) {
                    return false;
                }

                const BaseTypeInfo& typeInfo = GetBaseTypeInfo(variable->baseType);
                if (!typeInfo.IsValid()) {
                    return false;
                }

                return WriteBaseTypeToFixedBuffer(variable->baseType, dst, value);
            }
            catch (...) {
                return false;
            }
        }

        // Write with automatic bool -> uint32_t conversion
        bool write(const std::string& variableName, bool value) {
            if (!isValid()) {
                return false;
            }

            const BufferVariable* variable = findVariable(variableName);
            if (!variable || variable->baseType != BaseType::Bool) {
                return false;
            }

            if (!checkBounds(variable->offset, sizeof(uint32_t))) {
                return false;
            }

            uint32_t shaderBool = value ? 1u : 0u;
            uint8_t* dst = static_cast<uint8_t*>(m_data) + variable->offset;
            std::memcpy(dst, &shaderBool, sizeof(uint32_t));
            return true;
        }

        // Write a variable by name with type checking (base types)
        template<typename T>
        bool write(const std::string& variableName, const T& value) {
            static_assert(BaseTypeTraits<T>::supported, "Type not supported by BufferWriter");

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

            uint8_t* dst = static_cast<uint8_t*>(m_data) + variable->offset;

            // Special handling for bool (stored as uint32_t in shaders)
            if constexpr (std::is_same_v<T, bool>) {
                uint32_t shaderBool = value ? 1u : 0u;
                std::memcpy(dst, &shaderBool, sizeof(uint32_t));
            }
            else {
                std::memcpy(dst, &value, sizeof(T));
            }

            return true;
        }

        bool writeComposite(const std::string& variableName,
            std::shared_ptr<CompositeTypeInstance> instance) {
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

            if (!instance) {
                return false;
            }

            if (instance->GetDefinition()->GetTypeName() !=
                variable->composite->GetTypeName()) {
                return false;
            }

            if (!checkBounds(variable->offset, variable->size)) {
                return false;
            }

            uint8_t* dst = static_cast<uint8_t*>(m_data) + variable->offset;
            return instance->WriteToBuffer(dst);
        }

        // Write ShaderStruct specifically
        bool writeStruct(const std::string& variableName,
            std::shared_ptr<ShaderStructInstance> structValue) {
            if (!structValue || !structValue->IsStruct()) {
                return false;
            }
            return writeComposite(variableName,
                std::static_pointer_cast<CompositeTypeInstance>(structValue));
        }

        // Write ShaderArray specifically
        bool writeArray(const std::string& variableName,
            std::shared_ptr<ShaderArrayInstance> arrayValue) {
            if (!arrayValue || !arrayValue->IsArray()) {
                return false;
            }
            return writeComposite(variableName,
                std::static_pointer_cast<CompositeTypeInstance>(arrayValue));
        }

        // Helper: Check if a variable is composite
        bool isCompositeVariable(const std::string& variableName) const {
            const BufferVariable* variable = findVariable(variableName);
            return variable && variable->IsComposite();
        }

        // Helper: Check if a variable is base type
        bool isBaseVariable(const std::string& variableName) const {
            const BufferVariable* variable = findVariable(variableName);
            return variable && variable->IsBase();
        }
    };

} // namespace ShaderLib