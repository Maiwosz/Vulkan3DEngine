#include "pch.h"
#include "VulkanHelper.h"
#include <vulkan/vulkan.h>

namespace ShaderLib {

    uint32_t GetVulkanShaderStageFlags(StageFlags stageFlags) {
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

    uint32_t GetVulkanDescriptorType(DescriptorType type) {
        switch (type) {
        case DescriptorType::UniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::CombinedImageSampler:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::SeparateImage:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::SeparateSampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        default:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
    }

    uint32_t GetVulkanDescriptorTypeFromBufferType(BufferType bufferType) {
        switch (bufferType) {
        case BufferType::Uniform:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case BufferType::Storage:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        default:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
    }

    std::vector<uint32_t> GetSpirvForStage(const std::vector<CompiledStage>& stages, Stage stage) {
        for (const auto& compiledStage : stages) {
            if (compiledStage.stage == stage) {
                return compiledStage.spirv;
            }
        }
        return {};
    }

    std::vector<Stage> GetAvailableStages(const std::vector<CompiledStage>& stages) {
        std::vector<Stage> result;
        for (const auto& stage : stages) {
            result.push_back(stage.stage);
        }
        return result;
    }

    std::vector<VulkanDescriptorSetLayoutInfo> GetDescriptorSetLayoutsInfo(const ShaderMetadata& metadata) {
        std::map<uint32_t, VulkanDescriptorSetLayoutInfo> setMap;

        for (const auto& descriptor : metadata.descriptors) {
            if (setMap.find(descriptor.set) == setMap.end()) {
                VulkanDescriptorSetLayoutInfo layoutInfo;
                layoutInfo.setNumber = descriptor.set;
                setMap[descriptor.set] = layoutInfo;
            }

            VulkanDescriptorBindingInfo bindingInfo;
            bindingInfo.binding = descriptor.binding;
            bindingInfo.descriptorType = GetVulkanDescriptorType(descriptor.type);
            bindingInfo.descriptorCount = 1;
            bindingInfo.stageFlags = GetVulkanShaderStageFlags(descriptor.stages);

            setMap[descriptor.set].bindings.push_back(bindingInfo);
        }

        std::vector<VulkanDescriptorSetLayoutInfo> result;
        for (auto& pair : setMap) {
            result.push_back(pair.second);
        }

        return result;
    }

    std::vector<VulkanPushConstantRange> GetPushConstantRanges(const ShaderMetadata& metadata) {
        std::vector<VulkanPushConstantRange> ranges;

        for (const auto& pushConst : metadata.pushConstants) {
            VulkanPushConstantRange range;
            range.stageFlags = GetVulkanShaderStageFlags(pushConst.stages);
            range.offset = pushConst.offset;
            range.size = pushConst.size;
            ranges.push_back(range);
        }

        return ranges;
    }

    VulkanPipelineLayoutInfo GetPipelineLayoutInfo(const ShaderMetadata& metadata) {
        VulkanPipelineLayoutInfo layoutInfo;

        auto descriptorSets = GetDescriptorSetLayoutsInfo(metadata);
        for (const auto& set : descriptorSets) {
            layoutInfo.setLayoutIndices.push_back(set.setNumber);
        }

        layoutInfo.pushConstantRanges = GetPushConstantRanges(metadata);

        return layoutInfo;
    }

    VulkanBufferInfo GetGlobalUboInfo(const ShaderMetadata& metadata) {
        VulkanBufferInfo info;
        info.set = metadata.globalUBO.set;
        info.binding = metadata.globalUBO.binding;
        info.size = metadata.globalUBO.size;
        info.bufferType = metadata.globalUBO.bufferType;
        info.layoutStandard = metadata.globalUBO.layoutStandard;
        info.isDynamic = false;
        return info;
    }

    VulkanBufferInfo GetObjectUboInfo(const ShaderMetadata& metadata) {
        VulkanBufferInfo info;
        info.set = metadata.objectUBO.set;
        info.binding = metadata.objectUBO.binding;
        info.size = metadata.objectUBO.size;
        info.bufferType = metadata.objectUBO.bufferType;
        info.layoutStandard = metadata.objectUBO.layoutStandard;
        info.isDynamic = true; // Object UBO is typically dynamic
        return info;
    }

    std::vector<VulkanBufferInfo> GetCustomUboInfo(const ShaderMetadata& metadata) {
        std::vector<VulkanBufferInfo> buffers;

        for (const auto& ubo : metadata.customUBOs) {
            VulkanBufferInfo info;
            info.set = ubo.set;
            info.binding = ubo.binding;
            info.size = ubo.size;
            info.bufferType = ubo.bufferType;
            info.layoutStandard = ubo.layoutStandard;
            info.isDynamic = false;
            buffers.push_back(info);
        }

        return buffers;
    }

    std::vector<VulkanBufferInfo> GetCustomSsboInfo(const ShaderMetadata& metadata) {
        std::vector<VulkanBufferInfo> buffers;

        for (const auto& ssbo : metadata.customSSBOs) {
            VulkanBufferInfo info;
            info.set = ssbo.set;
            info.binding = ssbo.binding;
            info.size = ssbo.size;
            info.bufferType = ssbo.bufferType;
            info.layoutStandard = ssbo.layoutStandard;
            info.isDynamic = false;
            buffers.push_back(info);
        }

        return buffers;
    }

    std::vector<VulkanUniformVariableInfo> GetBufferVariables(const BufferObject& buffer) {
        std::vector<VulkanUniformVariableInfo> variables;

        for (const auto& var : buffer.variables) {
            VulkanUniformVariableInfo varInfo;
            varInfo.name = var.name;
            varInfo.type = var.type;
            varInfo.size = var.size;
            varInfo.offset = var.offset;
            varInfo.arraySize = var.arraySize;
            varInfo.typeName = var.typeName;
            variables.push_back(varInfo);
        }

        return variables;
    }

    std::vector<VulkanUniformVariableInfo> GetGlobalUboVariables(const ShaderMetadata& metadata) {
        return GetBufferVariables(metadata.globalUBO);
    }

    std::vector<VulkanUniformVariableInfo> GetObjectUboVariables(const ShaderMetadata& metadata) {
        return GetBufferVariables(metadata.objectUBO);
    }

    ShaderResourcesInfo GetShaderResourcesInfo(const ShaderMetadata& metadata) {
        ShaderResourcesInfo info;

        info.descriptorSets = GetDescriptorSetLayoutsInfo(metadata);
        info.pipelineLayout = GetPipelineLayoutInfo(metadata);
        info.availableStages = metadata.availableStages;

        if (metadata.usesGlobalUBO) {
            info.globalUBO = GetGlobalUboInfo(metadata);
            info.globalUboVariables = GetGlobalUboVariables(metadata);
        }

        if (metadata.usesObjectUBO) {
            info.objectUBO = GetObjectUboInfo(metadata);
            info.objectUboVariables = GetObjectUboVariables(metadata);
        }

        info.customUBOs = GetCustomUboInfo(metadata);
        info.customSSBOs = GetCustomSsboInfo(metadata);

        return info;
    }

    ShaderModulesInfo GetShaderModulesInfo(const std::vector<CompiledStage>& stages) {
        ShaderModulesInfo info;

        for (const auto& stage : stages) {
            info.modules.push_back({ stage.stage, stage.spirv });
        }

        return info;
    }

    CompleteShaderInfo GetCompleteShaderInfo(const ShaderData& shaderData, const ShaderMetadata& metadata) {
        CompleteShaderInfo info;
        info.resources = GetShaderResourcesInfo(metadata);
        info.modules = GetShaderModulesInfo(shaderData.stages);
        return info;
    }

} // namespace ShaderLib