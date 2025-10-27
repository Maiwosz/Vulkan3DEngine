#pragma once
#include "BufferAccessor.h"
#include "BufferIO.h"
#include "ShaderStruct.h"
#include "ShaderArray.h"
#include <cstring>

namespace ShaderLib {

    class BufferReader : public BufferAccessor {
    public:
        BufferReader() = default;

        void setBuffer(const BufferObject* bufferObject, const void* data, size_t dataSize) {
            m_bufferObject = bufferObject;
            m_data = const_cast<void*>(data);
            m_dataSize = dataSize;
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
                    auto composite = CloneComposite(variable->composite);
                    if (!composite) {
                        return false;
                    }

                    composite->InitializeData();
                    if (!composite->ReadFromBuffer(src)) {
                        return false;
                    }

                    // Convert CompositeType to appropriate derived type for BufferValue
                    if (composite->IsStruct()) {
                        outValue = std::static_pointer_cast<ShaderStruct>(composite);
                    }
                    else if (composite->IsArray()) {
                        outValue = std::static_pointer_cast<ShaderArray>(composite);
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

        // Read composite type (ShaderStruct or ShaderArray)
        bool readComposite(const std::string& variableName, std::shared_ptr<CompositeType>& outComposite) const {
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
                auto composite = CloneComposite(variable->composite);
                if (!composite) {
                    return false;
                }

                composite->InitializeData();
                if (!composite->ReadFromBuffer(src)) {
                    return false;
                }

                outComposite = composite;
                return true;
            }
            catch (...) {
                return false;
            }
        }

        // Read ShaderStruct specifically
        template<typename = void>
        bool readStruct(const std::string& variableName, std::shared_ptr<ShaderStruct>& outStruct) const {
            std::shared_ptr<CompositeType> composite;
            if (!readComposite(variableName, composite)) {
                return false;
            }

            if (!composite->IsStruct()) {
                return false;
            }

            outStruct = std::static_pointer_cast<ShaderStruct>(composite);
            return true;
        }

        // Read ShaderArray specifically
        template<typename = void>
        bool readArray(const std::string& variableName, std::shared_ptr<ShaderArray>& outArray) const {
            std::shared_ptr<CompositeType> composite;
            if (!readComposite(variableName, composite)) {
                return false;
            }

            if (!composite->IsArray()) {
                return false;
            }

            outArray = std::static_pointer_cast<ShaderArray>(composite);
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

    private:
        std::shared_ptr<CompositeType> CloneComposite(std::shared_ptr<CompositeType> src) const {
            return ShaderLib::CloneComposite(src);
        }
    };

} // namespace ShaderLib