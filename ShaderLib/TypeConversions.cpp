#include "pch.h"
#include "TypeConversions.h"
#include "Serialization.h"

namespace ShaderLib {
    namespace TypeConversion {

        Stage StringToStage(const std::string& str) {
            static const std::unordered_map<std::string, Stage> mapping{
                {"vertex", Stage::Vertex},
                {"fragment", Stage::Fragment},
                {"compute", Stage::Compute},
                {"geometry", Stage::Geometry},
                {"tess_control", Stage::TessellationControl},
                {"tess_eval", Stage::TessellationEvaluation}
            };

            auto it = mapping.find(str);
            if (it == mapping.end())
                throw std::runtime_error("Unknown shader stage: " + str);
            return it->second;
        }

        shaderc_shader_kind StageToShadercKind(Stage stage) {
            switch (stage) {
            case Stage::Vertex: return shaderc_glsl_vertex_shader;
            case Stage::Fragment: return shaderc_glsl_fragment_shader;
            case Stage::Compute: return shaderc_glsl_compute_shader;
            case Stage::Geometry: return shaderc_glsl_geometry_shader;
            case Stage::TessellationControl: return shaderc_glsl_tess_control_shader;
            case Stage::TessellationEvaluation: return shaderc_glsl_tess_evaluation_shader;
            default: throw std::runtime_error("Unsupported shader stage");
            }
        }

        DescriptorType SpirvTypeToDescriptorType(const spirv_cross::Compiler& compiler, const spirv_cross::SPIRType& type) {
            using namespace spv;
            switch (compiler.get_storage_class(type.self)) {
            case StorageClassUniform:
                if (type.basetype == spirv_cross::SPIRType::Struct)
                    return DescriptorType::UniformBuffer;
                break;
            case StorageClassStorageBuffer:
                return DescriptorType::StorageBuffer;
            case StorageClassUniformConstant:
                return DescriptorType::CombinedImageSampler;
            }
            return DescriptorType::UniformBuffer;
        }

        UniformType SPIRTypeToUniformType(const spirv_cross::SPIRType& type) {
            using BaseType = spirv_cross::SPIRType::BaseType;

            // Check for arrays first
            if (type.array.size() > 0) {
                return UniformType::Array;
            }

            switch (type.basetype) {
            case BaseType::Boolean:
                if (type.columns == 1 && type.vecsize == 1) {
                    return UniformType::Bool;
                }
                break;
            case BaseType::Float:
                if (type.columns == 1) {
                    switch (type.vecsize) {
                    case 1: return UniformType::Float;
                    case 2: return UniformType::Vec2;
                    case 3: return UniformType::Vec3;
                    case 4: return UniformType::Vec4;
                    }
                }
                else if (type.vecsize == type.columns) {
                    switch (type.columns) {
                    case 2: return UniformType::Mat2;
                    case 3: return UniformType::Mat3;
                    case 4: return UniformType::Mat4;
                    }
                }
                break;
            case BaseType::Int:
                if (type.columns == 1) {
                    switch (type.vecsize) {
                    case 1: return UniformType::Int;
                    case 2: return UniformType::IVec2;
                    case 3: return UniformType::IVec3;
                    case 4: return UniformType::IVec4;
                    }
                }
                break;
            case BaseType::UInt:
                if (type.columns == 1) {
                    switch (type.vecsize) {
                    case 1: return UniformType::UInt;
                    case 2: return UniformType::UVec2;
                    case 3: return UniformType::UVec3;
                    case 4: return UniformType::UVec4;
                    }
                }
                break;
            case BaseType::Double:
                if (type.columns == 1) {
                    switch (type.vecsize) {
                    case 1: return UniformType::Double;
                    case 2: return UniformType::DVec2;
                    case 3: return UniformType::DVec3;
                    case 4: return UniformType::DVec4;
                    }
                }
                break;
            case BaseType::Struct:
                return UniformType::Struct;
            default:
                break;
            }
            return UniformType::Unknown;
        }

        // Helper to compute size of array types
        uint32_t ComputeArraySize(const spirv_cross::SPIRType& type, const spirv_cross::Compiler& compiler) {
            if (type.array.empty()) return 0;

            uint32_t elementSize = 0;
            if (type.basetype == spirv_cross::SPIRType::Struct) {
                // Dla struktur używamy rozmiaru z uwzględnieniem paddingu
                elementSize = compiler.get_declared_struct_size(type);
            }
            else {
                // Pobieramy informacje o typie
                UniformType uniformType = SPIRTypeToUniformType(type);
                TypeInfo info = GetTypeInfo(uniformType);

                // Obliczamy wyrównany rozmiar elementu (uwzględniający padding)
                elementSize = info.size;
                uint32_t alignment = info.alignment;

                // Wyrównaj do wielokrotności alignmentu (std140)
                if (elementSize % alignment != 0) {
                    elementSize += alignment - (elementSize % alignment);
                }
            }

            // Mnożymy przez rozmiar tablicy (uwaga: SPIR-V może mieć tablice wielowymiarowe)
            return elementSize * type.array[0];
        }

        TypeInfo GetTypeInfo(UniformType type) {
            switch (type) {
            case UniformType::Bool: return { 4, 4 };
            case UniformType::Float: return { 4, 4 };
            case UniformType::Vec2: return { 8, 8 };
            case UniformType::Vec3: return { 12, 16 }; // Vec3 has 16-byte alignment in std140
            case UniformType::Vec4: return { 16, 16 };
            case UniformType::Mat2: return { 16, 8 };
            case UniformType::Mat3: return { 36, 16 }; // 3 vec3s, each 16-byte aligned
            case UniformType::Mat4: return { 64, 16 }; // 4 vec4s, each 16-byte aligned
            case UniformType::Int: return { 4, 4 };
            case UniformType::IVec2: return { 8, 8 };
            case UniformType::IVec3: return { 12, 16 };
            case UniformType::IVec4: return { 16, 16 };
            case UniformType::UInt: return { 4, 4 };
            case UniformType::UVec2: return { 8, 8 };
            case UniformType::UVec3: return { 12, 16 };
            case UniformType::UVec4: return { 16, 16 };
            case UniformType::Double: return { 8, 8 };
            case UniformType::DVec2: return { 16, 16 };
            case UniformType::DVec3: return { 24, 32 };
            case UniformType::DVec4: return { 32, 32 };
            default: return { 0, 16 }; // Default for struct or unknown
            }
        }

        std::string UniformTypeToString(UniformType type) {
            switch (type) {
            case UniformType::Bool: return "bool";
            case UniformType::Float: return "float";
            case UniformType::Vec2: return "vec2";
            case UniformType::Vec3: return "vec3";
            case UniformType::Vec4: return "vec4";
            case UniformType::Mat2: return "mat2";
            case UniformType::Mat3: return "mat3";
            case UniformType::Mat4: return "mat4";
            case UniformType::Int: return "int";
            case UniformType::IVec2: return "ivec2";
            case UniformType::IVec3: return "ivec3";
            case UniformType::IVec4: return "ivec4";
            case UniformType::UInt: return "uint";
            case UniformType::UVec2: return "uvec2";
            case UniformType::UVec3: return "uvec3";
            case UniformType::UVec4: return "uvec4";
            case UniformType::Double: return "double";
            case UniformType::DVec2: return "dvec2";
            case UniformType::DVec3: return "dvec3";
            case UniformType::DVec4: return "dvec4";
            case UniformType::Struct: return "struct";
            case UniformType::Array: return "array";
            default: return "unknown";
            }
        }

        UniformType StringToUniformType(const std::string& typeName) {
            static const std::unordered_map<std::string, UniformType> mapping{
                {"bool", UniformType::Bool},
                {"float", UniformType::Float},
                {"vec2", UniformType::Vec2},
                {"vec3", UniformType::Vec3},
                {"vec4", UniformType::Vec4},
                {"mat2", UniformType::Mat2},
                {"mat3", UniformType::Mat3},
                {"mat4", UniformType::Mat4},
                {"int", UniformType::Int},
                {"ivec2", UniformType::IVec2},
                {"ivec3", UniformType::IVec3},
                {"ivec4", UniformType::IVec4},
                {"uint", UniformType::UInt},
                {"uvec2", UniformType::UVec2},
                {"uvec3", UniformType::UVec3},
                {"uvec4", UniformType::UVec4},
                {"double", UniformType::Double},
                {"dvec2", UniformType::DVec2},
                {"dvec3", UniformType::DVec3},
                {"dvec4", UniformType::DVec4}
            };

            auto it = mapping.find(typeName);
            if (it != mapping.end())
                return it->second;
            return UniformType::Unknown;
        }
    }
}
