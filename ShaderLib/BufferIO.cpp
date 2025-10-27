#include "pch.h"
#include "BufferIO.h"
#include "ShaderStruct.h"
#include "ShaderArray.h"
#include <json.hpp>

namespace ShaderLib {

    BufferValue ReadBaseTypeFromBuffer(BaseType type, const void* src) {
        if (!src) {
            throw std::runtime_error("Source buffer is null");
        }

        switch (type) {
        case BaseType::Bool:      return BufferIO::ReadBool(src);
        case BaseType::Float:     return BufferIO::ReadSimpleType<float>(src);
        case BaseType::Int:       return BufferIO::ReadSimpleType<int32_t>(src);
        case BaseType::UInt:      return BufferIO::ReadSimpleType<uint32_t>(src);
        case BaseType::Double:    return BufferIO::ReadSimpleType<double>(src);

        case BaseType::Vec2:      return BufferIO::ReadSimpleType<glm::vec2>(src);
        case BaseType::Vec3:      return BufferIO::ReadSimpleType<glm::vec3>(src);
        case BaseType::Vec4:      return BufferIO::ReadSimpleType<glm::vec4>(src);

        case BaseType::IVec2:     return BufferIO::ReadSimpleType<glm::ivec2>(src);
        case BaseType::IVec3:     return BufferIO::ReadSimpleType<glm::ivec3>(src);
        case BaseType::IVec4:     return BufferIO::ReadSimpleType<glm::ivec4>(src);

        case BaseType::UVec2:     return BufferIO::ReadSimpleType<glm::uvec2>(src);
        case BaseType::UVec3:     return BufferIO::ReadSimpleType<glm::uvec3>(src);
        case BaseType::UVec4:     return BufferIO::ReadSimpleType<glm::uvec4>(src);

        case BaseType::DVec2:     return BufferIO::ReadSimpleType<glm::dvec2>(src);
        case BaseType::DVec3:     return BufferIO::ReadSimpleType<glm::dvec3>(src);
        case BaseType::DVec4:     return BufferIO::ReadSimpleType<glm::dvec4>(src);

        case BaseType::Mat2:      return BufferIO::ReadSimpleType<glm::mat2>(src);
        case BaseType::Mat3:      return BufferIO::ReadSimpleType<glm::mat3>(src);
        case BaseType::Mat4:      return BufferIO::ReadSimpleType<glm::mat4>(src);

        case BaseType::AtomicUInt: return BufferIO::ReadSimpleType<uint32_t>(src);

        default:
            throw std::runtime_error("Unsupported base type for reading");
        }
    }

    bool WriteBaseTypeToBuffer(BaseType type, void* dst, const BufferValue& value) {
        if (!dst) return false;

        switch (type) {
        case BaseType::Bool:      return BufferIO::WriteBool(dst, value);
        case BaseType::Float:     return BufferIO::WriteSimpleType<float>(dst, value);
        case BaseType::Int:       return BufferIO::WriteSimpleType<int32_t>(dst, value);
        case BaseType::UInt:      return BufferIO::WriteSimpleType<uint32_t>(dst, value);
        case BaseType::Double:    return BufferIO::WriteSimpleType<double>(dst, value);

        case BaseType::Vec2:      return BufferIO::WriteSimpleType<glm::vec2>(dst, value);
        case BaseType::Vec3:      return BufferIO::WriteSimpleType<glm::vec3>(dst, value);
        case BaseType::Vec4:      return BufferIO::WriteSimpleType<glm::vec4>(dst, value);

        case BaseType::IVec2:     return BufferIO::WriteSimpleType<glm::ivec2>(dst, value);
        case BaseType::IVec3:     return BufferIO::WriteSimpleType<glm::ivec3>(dst, value);
        case BaseType::IVec4:     return BufferIO::WriteSimpleType<glm::ivec4>(dst, value);

        case BaseType::UVec2:     return BufferIO::WriteSimpleType<glm::uvec2>(dst, value);
        case BaseType::UVec3:     return BufferIO::WriteSimpleType<glm::uvec3>(dst, value);
        case BaseType::UVec4:     return BufferIO::WriteSimpleType<glm::uvec4>(dst, value);

        case BaseType::DVec2:     return BufferIO::WriteSimpleType<glm::dvec2>(dst, value);
        case BaseType::DVec3:     return BufferIO::WriteSimpleType<glm::dvec3>(dst, value);
        case BaseType::DVec4:     return BufferIO::WriteSimpleType<glm::dvec4>(dst, value);

        case BaseType::Mat2:      return BufferIO::WriteSimpleType<glm::mat2>(dst, value);
        case BaseType::Mat3:      return BufferIO::WriteSimpleType<glm::mat3>(dst, value);
        case BaseType::Mat4:      return BufferIO::WriteSimpleType<glm::mat4>(dst, value);

        case BaseType::AtomicUInt: return BufferIO::WriteSimpleType<uint32_t>(dst, value);

        default:
            return false;
        }
    }

    std::shared_ptr<CompositeType> CloneComposite(std::shared_ptr<CompositeType> src) {
        if (!src) return nullptr;

        if (src->IsStruct()) {
            auto srcStruct = std::static_pointer_cast<ShaderStruct>(src);
            return std::make_shared<ShaderStruct>(*srcStruct);
        }
        else if (src->IsArray()) {
            auto srcArray = std::static_pointer_cast<ShaderArray>(src);
            return std::make_shared<ShaderArray>(*srcArray);
        }

        return nullptr;
    }

    bool WriteBaseTypeFromJson(BaseType type, std::vector<uint8_t>& dst, const nlohmann::json& jsonValue) {
        const auto& typeInfo = GetBaseTypeInfo(type);

        if (!typeInfo.IsValid()) {
            return false;
        }

        size_t startSize = dst.size();

        // Scalars
        if (typeInfo.IsScalar()) {
            switch (type) {
            case BaseType::Bool: {
                bool val = jsonValue.get<bool>();
                uint32_t shaderBool = val ? 1u : 0u;
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&shaderBool);
                dst.insert(dst.end(), bytes, bytes + sizeof(uint32_t));
                return true;
            }
            case BaseType::Float: {
                float val = jsonValue.get<float>();
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
                dst.insert(dst.end(), bytes, bytes + sizeof(float));
                return true;
            }
            case BaseType::Int: {
                int32_t val = jsonValue.get<int32_t>();
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
                dst.insert(dst.end(), bytes, bytes + sizeof(int32_t));
                return true;
            }
            case BaseType::UInt: {
                uint32_t val = jsonValue.get<uint32_t>();
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
                dst.insert(dst.end(), bytes, bytes + sizeof(uint32_t));
                return true;
            }
            case BaseType::Double: {
                double val = jsonValue.get<double>();
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
                dst.insert(dst.end(), bytes, bytes + sizeof(double));
                return true;
            }
            default:
                return false;
            }
        }
        // Vectors
        else if (typeInfo.IsVector()) {
            const uint32_t componentCount = typeInfo.components;

            // Float vectors
            if (type >= BaseType::Vec2 && type <= BaseType::Vec4) {
                std::vector<float> vec = jsonValue.get<std::vector<float>>();
                if (vec.size() != componentCount) return false;
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                dst.insert(dst.end(), bytes, bytes + componentCount * sizeof(float));
                return true;
            }
            // Int vectors
            else if (type >= BaseType::IVec2 && type <= BaseType::IVec4) {
                std::vector<int32_t> vec = jsonValue.get<std::vector<int32_t>>();
                if (vec.size() != componentCount) return false;
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                dst.insert(dst.end(), bytes, bytes + componentCount * sizeof(int32_t));
                return true;
            }
            // UInt vectors
            else if (type >= BaseType::UVec2 && type <= BaseType::UVec4) {
                std::vector<uint32_t> vec = jsonValue.get<std::vector<uint32_t>>();
                if (vec.size() != componentCount) return false;
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                dst.insert(dst.end(), bytes, bytes + componentCount * sizeof(uint32_t));
                return true;
            }
            // Double vectors
            else if (type >= BaseType::DVec2 && type <= BaseType::DVec4) {
                std::vector<double> vec = jsonValue.get<std::vector<double>>();
                if (vec.size() != componentCount) return false;
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                dst.insert(dst.end(), bytes, bytes + componentCount * sizeof(double));
                return true;
            }
        }
        // Matrices
        else if (typeInfo.IsMatrix()) {
            const uint32_t componentCount = typeInfo.components;
            std::vector<float> mat = jsonValue.get<std::vector<float>>();
            if (mat.size() != componentCount) return false;
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(mat.data());
            dst.insert(dst.end(), bytes, bytes + componentCount * sizeof(float));
            return true;
        }

        return false;
    }

} // namespace ShaderLib