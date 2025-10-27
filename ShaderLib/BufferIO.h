#pragma once
#include "ShaderTypes.h"
#include <cstring>
#include <json.hpp>

namespace ShaderLib {

    // Forward declarations
    class ShaderStruct;
    class ShaderArray;

    // ============================================================================
    // BUFFER I/O UTILITIES
    // ============================================================================

    namespace BufferIO {
        template<typename T>
        inline BufferValue ReadSimpleType(const void* src) {
            T value;
            std::memcpy(&value, src, sizeof(T));
            return BufferValue(value);
        }

        inline BufferValue ReadBool(const void* src) {
            uint32_t shaderBool;
            std::memcpy(&shaderBool, src, sizeof(uint32_t));
            return BufferValue(shaderBool != 0);
        }

        template<typename T>
        inline bool WriteSimpleType(void* dst, const BufferValue& value) {
            try {
                const T& val = std::get<T>(value);
                std::memcpy(dst, &val, sizeof(T));
                return true;
            }
            catch (const std::bad_variant_access&) {
                return false;
            }
        }

        inline bool WriteBool(void* dst, const BufferValue& value) {
            try {
                bool boolVal = std::get<bool>(value);
                uint32_t shaderBool = boolVal ? 1u : 0u;
                std::memcpy(dst, &shaderBool, sizeof(uint32_t));
                return true;
            }
            catch (const std::bad_variant_access&) {
                return false;
            }
        }
    } // namespace BufferIO

    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================

    BufferValue ReadBaseTypeFromBuffer(BaseType type, const void* src);
    bool WriteBaseTypeToBuffer(BaseType type, void* dst, const BufferValue& value);
    std::shared_ptr<CompositeType> CloneComposite(std::shared_ptr<CompositeType> src);
    bool WriteBaseTypeFromJson(BaseType type, std::vector<uint8_t>& dst, const nlohmann::json& jsonValue);
} // namespace ShaderLib