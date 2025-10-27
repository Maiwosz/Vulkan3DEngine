#pragma once
#include "BufferAccessor.h"
#include "BufferIO.h"
#include "ShaderStruct.h"
#include "ShaderArray.h"
#include <cstring>

namespace ShaderLib {

    class BufferWriter : public BufferAccessor {
    public:
        BufferWriter() = default;

        void setBuffer(const BufferObject* bufferObject, void* data, size_t dataSize) {
            m_bufferObject = bufferObject;
            m_data = data;
            m_dataSize = dataSize;
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
                    std::shared_ptr<CompositeType> composite = nullptr;

                    // Try to get ShaderStruct
                    if (auto structPtr = std::get_if<std::shared_ptr<ShaderStruct>>(&value)) {
                        composite = *structPtr;
                    }
                    // Try to get ShaderArray
                    else if (auto arrayPtr = std::get_if<std::shared_ptr<ShaderArray>>(&value)) {
                        composite = *arrayPtr;
                    }

                    if (!composite) {
                        return false;
                    }

                    // Validate type matches
                    if (composite->GetTypeName() != variable->composite->GetTypeName()) {
                        return false;
                    }

                    if (!composite->HasData()) {
                        return false;
                    }

                    return composite->WriteToBuffer(dst);
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

                return WriteBaseTypeToBuffer(variable->baseType, dst, value);
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

        // Write composite type (ShaderStruct or ShaderArray)
        bool writeComposite(const std::string& variableName, std::shared_ptr<CompositeType> composite) {
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

            if (!composite) {
                return false;
            }

            // Validate type matches
            if (composite->GetTypeName() != variable->composite->GetTypeName()) {
                return false;
            }

            if (!composite->HasData()) {
                return false;
            }

            if (!checkBounds(variable->offset, variable->size)) {
                return false;
            }

            uint8_t* dst = static_cast<uint8_t*>(m_data) + variable->offset;
            return composite->WriteToBuffer(dst);
        }

        // Write ShaderStruct specifically
        bool writeStruct(const std::string& variableName, std::shared_ptr<ShaderStruct> structValue) {
            if (!structValue || !structValue->IsStruct()) {
                return false;
            }
            return writeComposite(variableName, structValue);
        }

        // Write ShaderArray specifically
        bool writeArray(const std::string& variableName, std::shared_ptr<ShaderArray> arrayValue) {
            if (!arrayValue || !arrayValue->IsArray()) {
                return false;
            }
            return writeComposite(variableName, arrayValue);
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