#include "pch.h"
#include "TypeSerializationTable.h"
#include <cstring>

namespace ShaderLib {

    // ============================================================================
    // INTERNAL SERIALIZATION FUNCTIONS
    // ============================================================================

    namespace Internal {
        // Generic functions
        template<typename T>
        BaseTypeValue ReadFromBuffer_Generic(const void* src) {
            T value;
            std::memcpy(&value, src, sizeof(T));
            return BaseTypeValue(value);
        }

        template<typename T>
        bool WriteToFixedBuffer_Generic(void* dst, const BaseTypeValue& value) {
            try {
                const T& val = std::get<T>(value);
                std::memcpy(dst, &val, sizeof(T));
                return true;
            }
            catch (const std::bad_variant_access&) {
                return false;
            }
        }

        template<typename T>
        bool WriteToBuffer_Generic(std::vector<uint8_t>& dst, const BaseTypeValue& value) {
            try {
                const T& val = std::get<T>(value);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
                dst.insert(dst.end(), bytes, bytes + sizeof(T));
                return true;
            }
            catch (const std::bad_variant_access&) {
                return false;
            }
        }

        template<typename T>
        json ToJson_Scalar(const BaseTypeValue& value) {
            return std::get<T>(value);
        }

        template<typename T>
        BaseTypeValue FromJson_Scalar(const json& j) {
            return j.get<T>();
        }

        template<typename T>
        bool WriteFromJson_Scalar(std::vector<uint8_t>& dst, const json& j) {
            T value = j.get<T>();
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
            dst.insert(dst.end(), bytes, bytes + sizeof(T));
            return true;
        }

        // Bool specializations (stored as uint32_t in shaders)
        BaseTypeValue ReadFromBuffer_Bool(const void* src) {
            uint32_t shaderBool;
            std::memcpy(&shaderBool, src, sizeof(uint32_t));
            return BaseTypeValue(shaderBool != 0);
        }

        bool WriteToFixedBuffer_Bool(void* dst, const BaseTypeValue& value) {
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

        bool WriteToBuffer_Bool(std::vector<uint8_t>& dst, const BaseTypeValue& value) {
            try {
                bool boolVal = std::get<bool>(value);
                uint32_t shaderBool = boolVal ? 1u : 0u;
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&shaderBool);
                dst.insert(dst.end(), bytes, bytes + sizeof(uint32_t));
                return true;
            }
            catch (const std::bad_variant_access&) {
                return false;
            }
        }

        bool WriteFromJson_Bool(std::vector<uint8_t>& dst, const json& j) {
            bool val = j.get<bool>();
            uint32_t shaderBool = val ? 1u : 0u;
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&shaderBool);
            dst.insert(dst.end(), bytes, bytes + sizeof(uint32_t));
            return true;
        }

        // GLM vector helpers
        template<typename VecType>
        json ToJson_GlmVec(const BaseTypeValue& value) {
            const VecType& v = std::get<VecType>(value);
            json arr = json::array();
            for (int i = 0; i < v.length(); ++i) {
                arr.push_back(v[i]);
            }
            return arr;
        }

        template<typename VecType, typename ComponentType>
        BaseTypeValue FromJson_GlmVec(const json& j) {
            auto arr = j.get<std::vector<ComponentType>>();
            if (arr.size() != VecType::length()) {
                throw std::runtime_error("Invalid vector size");
            }
            VecType result;
            for (int i = 0; i < VecType::length(); ++i) {
                result[i] = arr[i];
            }
            return result;
        }

        template<typename VecType, typename ComponentType>
        bool WriteFromJson_GlmVec(std::vector<uint8_t>& dst, const json& j) {
            try {
                auto arr = j.get<std::vector<ComponentType>>();
                if (arr.size() != VecType::length()) return false;

                VecType vec;
                for (int i = 0; i < VecType::length(); ++i) {
                    vec[i] = arr[i];
                }

                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&vec);
                dst.insert(dst.end(), bytes, bytes + sizeof(VecType));
                return true;
            }
            catch (...) {
                return false;
            }
        }

        // GLM matrix helpers (column-major)
        template<typename MatType>
        json ToJson_GlmMat(const BaseTypeValue& value) {
            const MatType& m = std::get<MatType>(value);
            json arr = json::array();
            for (int col = 0; col < m.length(); ++col) {
                for (int row = 0; row < m[col].length(); ++row) {
                    arr.push_back(m[col][row]);
                }
            }
            return arr;
        }

        template<typename MatType>
        BaseTypeValue FromJson_GlmMat(const json& j) {
            constexpr int size = MatType::length() * MatType::length();
            auto arr = j.get<std::vector<float>>();
            if (arr.size() != size) {
                throw std::runtime_error("Invalid matrix size");
            }
            MatType result;
            int idx = 0;
            for (int col = 0; col < MatType::length(); ++col) {
                for (int row = 0; row < MatType::length(); ++row) {
                    result[col][row] = arr[idx++];
                }
            }
            return result;
        }

        template<typename MatType>
        bool WriteFromJson_GlmMat(std::vector<uint8_t>& dst, const json& j) {
            try {
                constexpr int size = MatType::length() * MatType::length();
                auto arr = j.get<std::vector<float>>();
                if (arr.size() != size) return false;

                MatType mat;
                int idx = 0;
                for (int col = 0; col < MatType::length(); ++col) {
                    for (int row = 0; row < MatType::length(); ++row) {
                        mat[col][row] = arr[idx++];
                    }
                }

                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&mat);
                dst.insert(dst.end(), bytes, bytes + sizeof(MatType));
                return true;
            }
            catch (...) {
                return false;
            }
        }

        // Concrete instantiations for each type
#define DEFINE_VEC_FUNCS(VecType, ComponentType) \
            BaseTypeValue FromJson_##VecType(const json& j) { \
                return FromJson_GlmVec<glm::VecType, ComponentType>(j); \
            } \
            json ToJson_##VecType(const BaseTypeValue& v) { \
                return ToJson_GlmVec<glm::VecType>(v); \
            } \
            bool WriteFromJson_##VecType(std::vector<uint8_t>& dst, const json& j) { \
                return WriteFromJson_GlmVec<glm::VecType, ComponentType>(dst, j); \
            }

        DEFINE_VEC_FUNCS(vec2, float)
            DEFINE_VEC_FUNCS(vec3, float)
            DEFINE_VEC_FUNCS(vec4, float)
            DEFINE_VEC_FUNCS(ivec2, int32_t)
            DEFINE_VEC_FUNCS(ivec3, int32_t)
            DEFINE_VEC_FUNCS(ivec4, int32_t)
            DEFINE_VEC_FUNCS(uvec2, uint32_t)
            DEFINE_VEC_FUNCS(uvec3, uint32_t)
            DEFINE_VEC_FUNCS(uvec4, uint32_t)
            DEFINE_VEC_FUNCS(dvec2, double)
            DEFINE_VEC_FUNCS(dvec3, double)
            DEFINE_VEC_FUNCS(dvec4, double)

#undef DEFINE_VEC_FUNCS

#define DEFINE_MAT_FUNCS(MatType) \
            BaseTypeValue FromJson_##MatType(const json& j) { \
                return FromJson_GlmMat<glm::MatType>(j); \
            } \
            json ToJson_##MatType(const BaseTypeValue& v) { \
                return ToJson_GlmMat<glm::MatType>(v); \
            } \
            bool WriteFromJson_##MatType(std::vector<uint8_t>& dst, const json& j) { \
                return WriteFromJson_GlmMat<glm::MatType>(dst, j); \
            }

            DEFINE_MAT_FUNCS(mat2)
            DEFINE_MAT_FUNCS(mat3)
            DEFINE_MAT_FUNCS(mat4)

#undef DEFINE_MAT_FUNCS

    } // namespace Internal

    // ============================================================================
    // SERIALIZATION TABLE DEFINITION
    // ============================================================================

    const BaseTypeSerializationInfo BASE_TYPE_SERIALIZATION_TABLE[] = {
        // Bool
        {
            BaseType::Bool,
            &Internal::ReadFromBuffer_Bool,
            &Internal::WriteToFixedBuffer_Bool,
            &Internal::WriteToBuffer_Bool,
            &Internal::ToJson_Scalar<bool>,
            &Internal::FromJson_Scalar<bool>,
            &Internal::WriteFromJson_Bool
        },

        // Float
        {
            BaseType::Float,
            &Internal::ReadFromBuffer_Generic<float>,
            &Internal::WriteToFixedBuffer_Generic<float>,
            &Internal::WriteToBuffer_Generic<float>,
            &Internal::ToJson_Scalar<float>,
            &Internal::FromJson_Scalar<float>,
            &Internal::WriteFromJson_Scalar<float>
        },

        // Int
        {
            BaseType::Int,
            &Internal::ReadFromBuffer_Generic<int32_t>,
            &Internal::WriteToFixedBuffer_Generic<int32_t>,
            &Internal::WriteToBuffer_Generic<int32_t>,
            &Internal::ToJson_Scalar<int32_t>,
            &Internal::FromJson_Scalar<int32_t>,
            &Internal::WriteFromJson_Scalar<int32_t>
        },

        // UInt
        {
            BaseType::UInt,
            &Internal::ReadFromBuffer_Generic<uint32_t>,
            &Internal::WriteToFixedBuffer_Generic<uint32_t>,
            &Internal::WriteToBuffer_Generic<uint32_t>,
            &Internal::ToJson_Scalar<uint32_t>,
            &Internal::FromJson_Scalar<uint32_t>,
            &Internal::WriteFromJson_Scalar<uint32_t>
        },

        // Double
        {
            BaseType::Double,
            &Internal::ReadFromBuffer_Generic<double>,
            &Internal::WriteToFixedBuffer_Generic<double>,
            &Internal::WriteToBuffer_Generic<double>,
            &Internal::ToJson_Scalar<double>,
            &Internal::FromJson_Scalar<double>,
            &Internal::WriteFromJson_Scalar<double>
        },

        // Vec2
        {
            BaseType::Vec2,
            &Internal::ReadFromBuffer_Generic<glm::vec2>,
            &Internal::WriteToFixedBuffer_Generic<glm::vec2>,
            &Internal::WriteToBuffer_Generic<glm::vec2>,
            &Internal::ToJson_vec2,
            &Internal::FromJson_vec2,
            &Internal::WriteFromJson_vec2
        },

        // Vec3
        {
            BaseType::Vec3,
            &Internal::ReadFromBuffer_Generic<glm::vec3>,
            &Internal::WriteToFixedBuffer_Generic<glm::vec3>,
            &Internal::WriteToBuffer_Generic<glm::vec3>,
            &Internal::ToJson_vec3,
            &Internal::FromJson_vec3,
            &Internal::WriteFromJson_vec3
		},

        // Vec4
        {
            BaseType::Vec4,
            &Internal::ReadFromBuffer_Generic<glm::vec4>,
            &Internal::WriteToFixedBuffer_Generic<glm::vec4>,
            &Internal::WriteToBuffer_Generic<glm::vec4>,
            &Internal::ToJson_vec4,
            &Internal::FromJson_vec4,
            &Internal::WriteFromJson_vec4
		},

        // Mat2
        {
            BaseType::Mat2,
            &Internal::ReadFromBuffer_Generic<glm::mat2>,
            &Internal::WriteToFixedBuffer_Generic<glm::mat2>,
            &Internal::WriteToBuffer_Generic<glm::mat2>,
            &Internal::ToJson_mat2,
            &Internal::FromJson_mat2,
            &Internal::WriteFromJson_mat2
        },

        // Mat3
        {
            BaseType::Mat3,
            &Internal::ReadFromBuffer_Generic<glm::mat3>,
            &Internal::WriteToFixedBuffer_Generic<glm::mat3>,
            &Internal::WriteToBuffer_Generic<glm::mat3>,
            &Internal::ToJson_mat3,
            &Internal::FromJson_mat3,
            &Internal::WriteFromJson_mat3
		},

        // Mat4
        {
            BaseType::Mat4,
            &Internal::ReadFromBuffer_Generic<glm::mat4>,
            &Internal::WriteToFixedBuffer_Generic<glm::mat4>,
            &Internal::WriteToBuffer_Generic<glm::mat4>,
            &Internal::ToJson_mat4,
            &Internal::FromJson_mat4,
            &Internal::WriteFromJson_mat4
        },

        // AtomicUInt
        {
            BaseType::AtomicUInt,
            &Internal::ReadFromBuffer_Generic<uint32_t>,
            &Internal::WriteToFixedBuffer_Generic<uint32_t>,
            &Internal::WriteToBuffer_Generic<uint32_t>,
            &Internal::ToJson_Scalar<uint32_t>,
            &Internal::FromJson_Scalar<uint32_t>,
            &Internal::WriteFromJson_Scalar<uint32_t>
        },

        // Unknown
        {
            BaseType::Unknown,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
        },
    };

} // namespace ShaderLib
