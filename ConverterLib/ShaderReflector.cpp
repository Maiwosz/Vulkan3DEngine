#include "pch.h"
#include "ShaderReflector.h"
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

using namespace ShaderLib;

namespace Shader {

    void ShaderReflector::ExtractPushConstants(
        const std::vector<uint32_t>& spirv,
        Stage stage,
        ShaderMetadata& metadata) {

        spirv_cross::Compiler compiler(spirv);
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        for (const auto& resource : resources.push_constant_buffers) {
            const auto& ranges = compiler.get_active_buffer_ranges(resource.id);
            uint32_t size = 0;

            for (const auto& range : ranges) {
                size = std::max<uint32_t>(size, range.offset + range.range);
            }

            PushConstantRange pcRange = {
                static_cast<StageFlags>(stage),
                0,
                size
            };

            // Check if we already have this push constant range
            bool found = false;
            for (auto& existing : metadata.pushConstants) {
                if (existing.offset == pcRange.offset && existing.size == pcRange.size) {
                    existing.stages |= pcRange.stages;
                    found = true;
                    break;
                }
            }

            if (!found) {
                metadata.pushConstants.push_back(pcRange);
            }
        }
    }

    void ShaderReflector::ExtractComputeInfo(
        const std::vector<uint32_t>& spirv,
        ComputeShaderInfo& computeInfo) {

        spirv_cross::Compiler compiler(spirv);
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        // Get workgroup size
        computeInfo.localSizeX = compiler.get_execution_mode_argument(
            spv::ExecutionModeLocalSize, 0);
        computeInfo.localSizeY = compiler.get_execution_mode_argument(
            spv::ExecutionModeLocalSize, 1);
        computeInfo.localSizeZ = compiler.get_execution_mode_argument(
            spv::ExecutionModeLocalSize, 2);

        // Detect shared memory usage
        for (const auto& resource : resources.storage_buffers) {
            auto storageClass = compiler.get_storage_class(resource.id);
            if (storageClass == spv::StorageClassWorkgroup) {
                computeInfo.usesSharedMemory = true;

                const auto& type = compiler.get_type(resource.type_id);
                computeInfo.sharedMemorySize += compiler.get_declared_struct_size(type);
            }
        }
    }

    void ShaderReflector::ProcessReflectionData(
        const std::vector<uint32_t>& spirv,
        Stage stage,
        ShaderMetadata& metadata) {

        // Extract push constants
        ExtractPushConstants(spirv, stage, metadata);

        // Extract compute shader specific info
        if (stage == Stage::Compute) {
            ComputeShaderInfo computeInfo;
            ExtractComputeInfo(spirv, computeInfo);
            metadata.computeInfo = computeInfo;
        }
    }

} // namespace Shader