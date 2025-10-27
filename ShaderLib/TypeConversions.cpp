#include "pch.h"
#include "TypeConversions.h"

namespace ShaderLib {
    namespace TypeConversion {

        // ============================================================================
        // STAGE CONVERSIONS
        // ============================================================================

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

        std::string StageToString(Stage stage) {
            switch (stage) {
            case Stage::Vertex: return "vertex";
            case Stage::Fragment: return "fragment";
            case Stage::Compute: return "compute";
            case Stage::Geometry: return "geometry";
            case Stage::TessellationControl: return "tess_control";
            case Stage::TessellationEvaluation: return "tess_eval";
            default: return "unknown";
            }
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

        // ============================================================================
        // SPIR-V TYPE CONVERSIONS
        // ============================================================================

        DescriptorType SpirvTypeToDescriptorType(
            const spirv_cross::Compiler& compiler,
            const spirv_cross::SPIRType& type
        ) {
            using namespace spv;

            switch (compiler.get_storage_class(type.self)) {
            case StorageClassUniform:
                if (type.basetype == spirv_cross::SPIRType::Struct)
                    return DescriptorType::UniformBuffer;
                break;

            case StorageClassStorageBuffer:
                return DescriptorType::StorageBuffer;

            case StorageClassUniformConstant:
                // Distinguish between different sampler/image types
                if (type.basetype == spirv_cross::SPIRType::Image) {
                    const auto& image = type.image;

                    // Check if it's a storage image
                    if (image.sampled == 2) {
                        if (image.arrayed)
                            return DescriptorType::Image2DArray;
                        return DescriptorType::Image2D;
                    }

                    // Check for shadow samplers
                    if (image.depth) {
                        return DescriptorType::Sampler2DShadow;
                    }

                    // Regular samplers
                    if (image.dim == spv::DimCube) {
                        return image.arrayed
                            ? DescriptorType::SamplerCubeArray
                            : DescriptorType::SamplerCube;
                    }

                    if (image.dim == spv::Dim2D) {
                        return image.arrayed
                            ? DescriptorType::Sampler2DArray
                            : DescriptorType::Sampler2D;
                    }
                }

                // Check for separate sampler
                if (type.basetype == spirv_cross::SPIRType::Sampler)
                    return DescriptorType::Sampler;

                break;

            case StorageClassInput:
                // Check for input attachments (subpass inputs)
                if (type.basetype == spirv_cross::SPIRType::Image &&
                    type.image.dim == spv::DimSubpassData)
                    return DescriptorType::InputAttachment;
                break;
            }

            return DescriptorType::Unknown;
        }

        BaseType SPIRTypeToBaseType(const spirv_cross::SPIRType& type) {
            using SPIRBaseType = spirv_cross::SPIRType::BaseType;

            // Check for arrays
            if (type.array.size() > 0) {
                return BaseType::Array;
            }

            // Check for structs
            if (type.basetype == SPIRBaseType::Struct) {
                return BaseType::Struct;
            }

            // Handle scalar and vector types
            switch (type.basetype) {
            case SPIRBaseType::Boolean:
                if (type.columns == 1 && type.vecsize == 1)
                    return BaseType::Bool;
                break;

            case SPIRBaseType::Float:
                if (type.columns == 1) {
                    switch (type.vecsize) {
                    case 1: return BaseType::Float;
                    case 2: return BaseType::Vec2;
                    case 3: return BaseType::Vec3;
                    case 4: return BaseType::Vec4;
                    }
                }
                else if (type.vecsize == type.columns) {
                    // Square matrices
                    switch (type.columns) {
                    case 2: return BaseType::Mat2;
                    case 3: return BaseType::Mat3;
                    case 4: return BaseType::Mat4;
                    }
                }
                break;

            case SPIRBaseType::Int:
                if (type.columns == 1) {
                    switch (type.vecsize) {
                    case 1: return BaseType::Int;
                    case 2: return BaseType::IVec2;
                    case 3: return BaseType::IVec3;
                    case 4: return BaseType::IVec4;
                    }
                }
                break;

            case SPIRBaseType::UInt:
                if (type.columns == 1) {
                    switch (type.vecsize) {
                    case 1: return BaseType::UInt;
                    case 2: return BaseType::UVec2;
                    case 3: return BaseType::UVec3;
                    case 4: return BaseType::UVec4;
                    }
                }
                break;

            case SPIRBaseType::Double:
                if (type.columns == 1) {
                    switch (type.vecsize) {
                    case 1: return BaseType::Double;
                    case 2: return BaseType::DVec2;
                    case 3: return BaseType::DVec3;
                    case 4: return BaseType::DVec4;
                    }
                }
                break;

            case SPIRBaseType::AtomicCounter:
                return BaseType::AtomicUInt;

            default:
                break;
            }

            return BaseType::Unknown;
        }

        // ============================================================================
        // ARRAY SIZE COMPUTATION
        // ============================================================================

        uint32_t ComputeArraySize(
            const spirv_cross::SPIRType& type,
            const spirv_cross::Compiler& compiler,
            LayoutStandard standard
        ) {
            if (type.array.empty())
                return 0;

            uint32_t arrayCount = type.array[0];

            if (type.basetype == spirv_cross::SPIRType::Struct) {
                // For structs, use the declared struct size with proper alignment
                uint32_t structSize = compiler.get_declared_struct_size(type);
                uint32_t baseAlignment = structSize; // Conservative estimate
                uint32_t alignment = GetArrayElementAlignment(baseAlignment, standard);
                uint32_t alignedSize = AlignTo(structSize, alignment);
                return alignedSize * arrayCount;
            }
            else {
                // For base types, use the type system
                BaseType baseType = SPIRTypeToBaseType(type);
                if (baseType != BaseType::Unknown && baseType != BaseType::Array) {
                    return CalculateArraySize(baseType, arrayCount, standard);
                }

                // Fallback for unknown types
                uint32_t elementSize = compiler.get_declared_struct_size(type);
                return elementSize * arrayCount;
            }
        }

        // ============================================================================
        // STORAGE CLASS CONVERSIONS
        // ============================================================================

        DescriptorType StorageClassToDescriptorType(spv::StorageClass storageClass) {
            switch (storageClass) {
            case spv::StorageClassUniform:
                return DescriptorType::UniformBuffer;
            case spv::StorageClassStorageBuffer:
                return DescriptorType::StorageBuffer;
            case spv::StorageClassUniformConstant:
                return DescriptorType::Sampler2D; // Default, needs context
            default:
                return DescriptorType::Unknown;
            }
        }

    } // namespace TypeConversion
} // namespace ShaderLib