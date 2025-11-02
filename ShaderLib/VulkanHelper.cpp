#include "pch.h"
#include "VulkanHelper.h"
#include <vulkan/vulkan.h>

namespace ShaderLib {

    // ============================================================================
    // CONVERSION FUNCTIONS
    // ============================================================================

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
    // QUERY FUNCTIONS
    // ============================================================================

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
        result.reserve(stages.size());
        for (const auto& stage : stages) {
            result.push_back(stage.stage);
        }
        return result;
    }

    // ============================================================================
    // DESCRIPTOR AND LAYOUT INFO FUNCTIONS
    // ============================================================================

    std::vector<VulkanDescriptorSetLayoutInfo> GetDescriptorSetLayoutsInfo(const ShaderMetadata& metadata) {
        std::map<uint32_t, VulkanDescriptorSetLayoutInfo> setMap;

        // Iterate through all descriptor sets
        for (const auto& descriptorSet : metadata.descriptorSets) {
            if (setMap.find(descriptorSet.setNumber) == setMap.end()) {
                VulkanDescriptorSetLayoutInfo layoutInfo;
                layoutInfo.setNumber = descriptorSet.setNumber;
                setMap[descriptorSet.setNumber] = layoutInfo;
            }

            // Process each slot in the descriptor set
            for (const auto& slot : descriptorSet.slots) {
                VulkanDescriptorBindingInfo bindingInfo;
                bindingInfo.binding = slot.binding;
                bindingInfo.descriptorType = GetVulkanDescriptorType(slot.type);
                bindingInfo.descriptorCount = 1;
                bindingInfo.stageFlags = GetVulkanShaderStageFlags(slot.stages);

                setMap[descriptorSet.setNumber].bindings.push_back(bindingInfo);
            }
        }

        std::vector<VulkanDescriptorSetLayoutInfo> result;
        result.reserve(setMap.size());
        for (auto& pair : setMap) {
            result.push_back(std::move(pair.second));
        }

        return result;
    }

    std::vector<VulkanPushConstantRange> GetPushConstantRanges(const ShaderMetadata& metadata) {
        std::vector<VulkanPushConstantRange> ranges;
        ranges.reserve(metadata.pushConstants.size());

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
        layoutInfo.setLayoutIndices.reserve(descriptorSets.size());
        for (const auto& set : descriptorSets) {
            layoutInfo.setLayoutIndices.push_back(set.setNumber);
        }

        layoutInfo.pushConstantRanges = GetPushConstantRanges(metadata);

        return layoutInfo;
    }

    // ============================================================================
    // BUFFER INFO FUNCTIONS
    // ============================================================================

    VulkanBufferInfo GetGlobalUboInfo(const ShaderMetadata& metadata) {
        VulkanBufferInfo info;
        info.set = GLOBAL_DESCRIPTOR_SET;
        info.binding = GLOBAL_UBO_BINDING;
        info.size = metadata.globalUBO.size;
        info.bufferType = metadata.globalUBO.bufferType;
        info.layoutStandard = metadata.globalUBO.layoutStandard;
        info.isDynamic = false;
        return info;
    }

    VulkanBufferInfo GetObjectUboInfo(const ShaderMetadata& metadata) {
        VulkanBufferInfo info;
        info.set = OBJECT_DESCRIPTOR_SET;
        info.binding = OBJECT_UBO_BINDING;
        info.size = metadata.objectUBO.size;
        info.bufferType = metadata.objectUBO.bufferType;
        info.layoutStandard = metadata.objectUBO.layoutStandard;
        info.isDynamic = true;
        return info;
    }

    std::vector<VulkanBufferInfo> GetCustomBuffersInfo(const ShaderMetadata& metadata) {
        std::vector<VulkanBufferInfo> buffers;

        // Iterate through all descriptor sets
        for (const auto& descriptorSet : metadata.descriptorSets) {
            // Get all buffers from this set
            auto setBuffers = descriptorSet.GetAllBuffers();

            for (const auto* buffer : setBuffers) {
                // Find the corresponding slot to get binding information
                for (const auto& slot : descriptorSet.slots) {
                    if (slot.name == buffer->name && slot.IsBuffer()) {
                        VulkanBufferInfo info;
                        info.set = descriptorSet.setNumber;
                        info.binding = slot.binding;
                        info.size = buffer->size;
                        info.bufferType = buffer->bufferType;
                        info.layoutStandard = buffer->layoutStandard;
                        info.isDynamic = false;
                        buffers.push_back(info);
                        break;
                    }
                }
            }
        }

        return buffers;
    }

    // ============================================================================
    // VARIABLE INFO FUNCTIONS
    // ============================================================================

    std::vector<VulkanBufferVariableInfo> GetBufferVariables(const BufferObject& buffer) {
        std::vector<VulkanBufferVariableInfo> variables;
        variables.reserve(buffer.variables.size());

        for (const auto& var : buffer.variables) {
            VulkanBufferVariableInfo varInfo;
            varInfo.name = var.name;
            varInfo.baseType = var.baseType;
            varInfo.size = var.size;
            varInfo.offset = var.offset;
            variables.push_back(varInfo);
        }

        return variables;
    }

    std::vector<VulkanBufferVariableInfo> GetGlobalUboVariables(const ShaderMetadata& metadata) {
        return GetBufferVariables(metadata.globalUBO);
    }

    std::vector<VulkanBufferVariableInfo> GetObjectUboVariables(const ShaderMetadata& metadata) {
        return GetBufferVariables(metadata.objectUBO);
    }

    // ============================================================================
    // COMPLETE INFO FUNCTIONS
    // ============================================================================

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

        info.customBuffers = GetCustomBuffersInfo(metadata);

        return info;
    }

    ShaderModulesInfo GetShaderModulesInfo(const std::vector<CompiledStage>& stages) {
        ShaderModulesInfo info;
        info.modules.reserve(stages.size());

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