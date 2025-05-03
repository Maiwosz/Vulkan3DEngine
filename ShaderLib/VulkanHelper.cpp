#include "pch.h"
#include "VulkanHelper.h" 
#include "ShaderLib.h"
#include <algorithm>
#include <utility>
#include <cassert>
#include <set>

namespace ShaderLib {

    // Convert ShaderLib::Stage to Vulkan shader stage flags (as uint32_t)
    uint32_t GetVulkanShaderStageFlags(StageFlags stageFlags) {
        uint32_t result = 0;

        if (stageFlags & static_cast<uint32_t>(Stage::Vertex))
            result |= 0x00000001; // VK_SHADER_STAGE_VERTEX_BIT
        if (stageFlags & static_cast<uint32_t>(Stage::Fragment))
            result |= 0x00000010; // VK_SHADER_STAGE_FRAGMENT_BIT
        if (stageFlags & static_cast<uint32_t>(Stage::Compute))
            result |= 0x00000020; // VK_SHADER_STAGE_COMPUTE_BIT
        if (stageFlags & static_cast<uint32_t>(Stage::Geometry))
            result |= 0x00000008; // VK_SHADER_STAGE_GEOMETRY_BIT
        if (stageFlags & static_cast<uint32_t>(Stage::TessellationControl))
            result |= 0x00000002; // VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
        if (stageFlags & static_cast<uint32_t>(Stage::TessellationEvaluation))
            result |= 0x00000004; // VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT

        return result;
    }

    // Convert ShaderLib::DescriptorType to Vulkan descriptor type (as uint32_t)
    uint32_t GetVulkanDescriptorType(DescriptorType type) {
        switch (type) {
        case DescriptorType::UniformBuffer:
            return 6; // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        case DescriptorType::StorageBuffer:
            return 7; // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        case DescriptorType::CombinedImageSampler:
            return 1; // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        case DescriptorType::SeparateImage:
            return 2; // VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
        case DescriptorType::SeparateSampler:
            return 0; // VK_DESCRIPTOR_TYPE_SAMPLER
        default:
            return 6; // Default to uniform buffer
        }
    }

    // Extract SPIR-V code for a specific shader stage
    std::vector<uint32_t> GetSpirvForStage(const std::vector<CompiledStage>& stages, Stage stage) {
        for (const auto& compiledStage : stages) {
            if (compiledStage.stage == stage) {
                return compiledStage.spirv;
            }
        }
        return {};
    }

    // Get all available shader stages from compiled stages
    std::vector<Stage> GetAvailableStages(const std::vector<CompiledStage>& stages) {
        std::vector<Stage> result;
        for (const auto& stage : stages) {
            result.push_back(stage.stage);
        }
        return result;
    }

    // Extract descriptor binding info from ShaderMetadata for all sets
    std::vector<VulkanDescriptorSetLayoutInfo> GetDescriptorSetLayoutsInfo(const ShaderMetadata& metadata) {
        // First, identify all unique set numbers
        std::set<uint32_t> setNumbers;
        for (const auto& descriptor : metadata.descriptors) {
            setNumbers.insert(descriptor.set);
        }

        // Create descriptor set info for each set
        std::vector<VulkanDescriptorSetLayoutInfo> result;
        for (uint32_t setNumber : setNumbers) {
            VulkanDescriptorSetLayoutInfo setInfo;
            setInfo.setNumber = setNumber;

            // Add all bindings for this set
            for (const auto& descriptor : metadata.descriptors) {
                if (descriptor.set == setNumber) {
                    VulkanDescriptorBindingInfo bindingInfo;
                    bindingInfo.binding = descriptor.binding;
                    bindingInfo.descriptorType = GetVulkanDescriptorType(descriptor.type);
                    bindingInfo.descriptorCount = 1; // Default to 1, adjust if arrays are needed
                    bindingInfo.stageFlags = GetVulkanShaderStageFlags(descriptor.stages);

                    setInfo.bindings.push_back(bindingInfo);
                }
            }

            // Sort bindings by binding number
            std::sort(setInfo.bindings.begin(), setInfo.bindings.end(),
                [](const VulkanDescriptorBindingInfo& a, const VulkanDescriptorBindingInfo& b) {
                    return a.binding < b.binding;
                });

            result.push_back(setInfo);
        }

        // Sort sets by set number
        std::sort(result.begin(), result.end(),
            [](const VulkanDescriptorSetLayoutInfo& a, const VulkanDescriptorSetLayoutInfo& b) {
                return a.setNumber < b.setNumber;
            });

        return result;
    }

    // Extract push constant ranges from ShaderMetadata
    std::vector<VulkanPushConstantRange> GetPushConstantRanges(const ShaderMetadata& metadata) {
        std::vector<VulkanPushConstantRange> ranges;

        for (const auto& pcRange : metadata.pushConstants) {
            VulkanPushConstantRange range;
            range.stageFlags = GetVulkanShaderStageFlags(pcRange.stages);
            range.offset = pcRange.offset;
            range.size = pcRange.size;

            ranges.push_back(range);
        }

        return ranges;
    }

    // Get pipeline layout information
    VulkanPipelineLayoutInfo GetPipelineLayoutInfo(const ShaderMetadata& metadata) {
        VulkanPipelineLayoutInfo info;

        // Add all descriptor set layout indices
        std::vector<VulkanDescriptorSetLayoutInfo> setLayouts = GetDescriptorSetLayoutsInfo(metadata);
        for (const auto& layout : setLayouts) {
            info.setLayoutIndices.push_back(layout.setNumber);
        }

        // Add push constant ranges
        info.pushConstantRanges = GetPushConstantRanges(metadata);

        return info;
    }

    // Extract uniform buffer information for GlobalUBO
    VulkanUniformBufferInfo GetGlobalUboInfo(const ShaderMetadata& metadata) {
        VulkanUniformBufferInfo info;
        if (metadata.usesGlobalUBO) {
            info.set = metadata.globalUBO.set;
            info.binding = metadata.globalUBO.binding;
            info.size = metadata.globalUBO.size;
            info.isDynamic = false;
        }
        else {
            info.set = GLOBAL_DESCRIPTOR_SET;
            info.binding = 0;
            info.size = 0;
            info.isDynamic = false;
        }
        return info;
    }

    // Extract uniform buffer information for ObjectUBO
    VulkanUniformBufferInfo GetObjectUboInfo(const ShaderMetadata& metadata) {
        VulkanUniformBufferInfo info;
        if (metadata.usesObjectUBO) {
            info.set = metadata.objectUBO.set;
            info.binding = metadata.objectUBO.binding;
            info.size = metadata.objectUBO.size;
            info.isDynamic = true; // Object UBOs are typically dynamic
        }
        else {
            info.set = OBJECT_DESCRIPTOR_SET;
            info.binding = 0;
            info.size = 0;
            info.isDynamic = false;
        }
        return info;
    }

    // Extract custom uniform buffer information
    std::vector<VulkanUniformBufferInfo> GetCustomUboInfo(const ShaderMetadata& metadata) {
        std::vector<VulkanUniformBufferInfo> result;

        for (const auto& ubo : metadata.customUBOs) {
            VulkanUniformBufferInfo info;
            info.set = ubo.set;
            info.binding = ubo.binding;
            info.size = ubo.size;
            info.isDynamic = false; // Default for custom UBOs

            result.push_back(info);
        }

        return result;
    }

    // Get information about uniform variables in GlobalUBO
    std::vector<VulkanUniformVariableInfo> GetGlobalUboVariables(const ShaderMetadata& metadata) {
        std::vector<VulkanUniformVariableInfo> result;

        if (metadata.usesGlobalUBO) {
            for (const auto& var : metadata.globalUBO.variables) {
                VulkanUniformVariableInfo varInfo;
                varInfo.name = var.name;
                varInfo.type = var.type;
                varInfo.size = var.size;
                varInfo.offset = var.offset;
                varInfo.arraySize = var.arraySize;
                varInfo.typeName = var.typeName;

                result.push_back(varInfo);
            }
        }

        return result;
    }

    // Get information about uniform variables in ObjectUBO
    std::vector<VulkanUniformVariableInfo> GetObjectUboVariables(const ShaderMetadata& metadata) {
        std::vector<VulkanUniformVariableInfo> result;

        if (metadata.usesObjectUBO) {
            for (const auto& var : metadata.objectUBO.variables) {
                VulkanUniformVariableInfo varInfo;
                varInfo.name = var.name;
                varInfo.type = var.type;
                varInfo.size = var.size;
                varInfo.offset = var.offset;
                varInfo.arraySize = var.arraySize;
                varInfo.typeName = var.typeName;

                result.push_back(varInfo);
            }
        }

        return result;
    }

    // Main function to extract all resources information from ShaderMetadata
    ShaderResourcesInfo GetShaderResourcesInfo(const ShaderMetadata& metadata) {
        ShaderResourcesInfo info;

        // Extract descriptor sets
        info.descriptorSets = GetDescriptorSetLayoutsInfo(metadata);

        // Extract pipeline layout
        info.pipelineLayout = GetPipelineLayoutInfo(metadata);

        // Extract UBO information
        info.globalUBO = GetGlobalUboInfo(metadata);
        info.objectUBO = GetObjectUboInfo(metadata);
        info.customUBOs = GetCustomUboInfo(metadata);

        // Extract uniform variables
        info.globalUboVariables = GetGlobalUboVariables(metadata);
        info.objectUboVariables = GetObjectUboVariables(metadata);

        // Available stages
        info.availableStages = metadata.availableStages;

        return info;
    }

    // Extract shader modules information from compiled stages
    ShaderModulesInfo GetShaderModulesInfo(const std::vector<CompiledStage>& stages) {
        ShaderModulesInfo info;

        for (const auto& stage : stages) {
            info.modules.emplace_back(stage.stage, stage.spirv);
        }

        return info;
    }

    // Extract complete shader information from ShaderData
    CompleteShaderInfo GetCompleteShaderInfo(const ShaderData& shaderData, const ShaderMetadata& metadata) {
        CompleteShaderInfo info;

        // Extract resources information from metadata
        info.resources = GetShaderResourcesInfo(metadata);

        // Extract modules information from compiled stages
        info.modules = GetShaderModulesInfo(shaderData.stages);

        return info;
    }

} // namespace ShaderLib