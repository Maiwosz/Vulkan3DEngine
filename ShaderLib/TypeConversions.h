#pragma once
#include "ShaderLib.h"
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include <shaderc/shaderc.hpp>

namespace ShaderLib {
    namespace TypeConversion {

        // ============================================================================
        // STAGE CONVERSIONS
        // ============================================================================

        inline Stage StringToStage(const std::string& str) {
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

        inline std::string StageToString(Stage stage) {
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

        inline shaderc_shader_kind StageToShadercKind(Stage stage) {
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

        inline uint32_t StageToVulkan(StageFlags stageFlags) {
            uint32_t vulkanFlags = 0;

            if (stageFlags & static_cast<uint32_t>(Stage::Vertex))
                vulkanFlags |= VK_SHADER_STAGE_VERTEX_BIT;
            if (stageFlags & static_cast<uint32_t>(Stage::Fragment))
                vulkanFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            if (stageFlags & static_cast<uint32_t>(Stage::Compute))
                vulkanFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
            if (stageFlags & static_cast<uint32_t>(Stage::Geometry))
                vulkanFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
            if (stageFlags & static_cast<uint32_t>(Stage::TessellationControl))
                vulkanFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            if (stageFlags & static_cast<uint32_t>(Stage::TessellationEvaluation))
                vulkanFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

            return vulkanFlags;
        }

        // ============================================================================
        // DESCRIPTOR TYPE CONVERSIONS
        // ============================================================================

        inline DescriptorType StorageClassToDescriptorType(spv::StorageClass storageClass) {
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

        inline uint32_t DescriptorTypeToVulkan(DescriptorType type) {
            const DescriptorTypeInfo& info = GetDescriptorTypeInfo(type);

            if (info.IsBuffer()) {
                if (type == DescriptorType::UniformBuffer)
                    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                if (type == DescriptorType::StorageBuffer)
                    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            }

            if (info.IsSampler() && !info.IsTexture())
                return VK_DESCRIPTOR_TYPE_SAMPLER;

            if (info.IsTexture())
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

            if (info.IsImage())
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

            if (type == DescriptorType::InputAttachment)
                return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }

        // ============================================================================
        // BUFFER TYPE DETECTION
        // ============================================================================

        inline BufferType StorageClassToBufferType(spv::StorageClass storageClass) {
            switch (storageClass) {
            case spv::StorageClassUniform:
                return BufferType::Uniform;
            case spv::StorageClassStorageBuffer:
                return BufferType::Storage;
            default:
                return BufferType::Uniform; // Default fallback
            }
        }

        inline LayoutStandard BufferTypeToLayoutStandard(BufferType bufferType) {
            return (bufferType == BufferType::Storage)
                ? LayoutStandard::Std430
                : LayoutStandard::Std140;
        }

        // ============================================================================
        // BUFFER ACCESS MODE
        // ============================================================================

        inline const char* BufferAccessModeToString(BufferAccessMode mode) {
            switch (mode) {
            case BufferAccessMode::ReadOnly: return "readonly";
            case BufferAccessMode::WriteOnly: return "writeonly";
            case BufferAccessMode::ReadWrite: return "readwrite";
            default: return "unknown";
            }
        }

    } // namespace TypeConversion
} // namespace ShaderLib