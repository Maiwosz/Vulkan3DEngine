#pragma once

#include "ShaderLib.h"
#include <vector>
#include <utility>
#include <string>
#include <cstdint>

namespace ShaderLib {

    // Forward declarations
    struct VulkanDescriptorBindingInfo;
    struct VulkanPushConstantRange;
    struct VulkanDescriptorSetLayoutInfo;
    struct VulkanPipelineLayoutInfo;
    struct VulkanUniformBufferInfo;
    struct VulkanUniformVariableInfo;
    struct ShaderResourcesInfo;
    struct ShaderModulesInfo;
    struct CompleteShaderInfo;

    // Convert ShaderLib::Stage to Vulkan shader stage flags
    uint32_t GetVulkanShaderStageFlags(StageFlags stageFlags);

    // Convert ShaderLib::DescriptorType to Vulkan descriptor type
    uint32_t GetVulkanDescriptorType(DescriptorType type);

    // Structure to hold Vulkan descriptor binding information
    struct VulkanDescriptorBindingInfo {
        uint32_t binding;
        uint32_t descriptorType;
        uint32_t descriptorCount;
        uint32_t stageFlags;
    };

    // Structure to hold Vulkan push constant range information
    struct VulkanPushConstantRange {
        uint32_t stageFlags;
        uint32_t offset;
        uint32_t size;
    };

    // Structure to hold Vulkan descriptor set layout information
    struct VulkanDescriptorSetLayoutInfo {
        uint32_t setNumber;
        std::vector<VulkanDescriptorBindingInfo> bindings;
    };

    // Structure to hold Vulkan pipeline layout information
    struct VulkanPipelineLayoutInfo {
        std::vector<uint32_t> setLayoutIndices;
        std::vector<VulkanPushConstantRange> pushConstantRanges;
    };

    // Structure for uniform buffer creation information
    struct VulkanUniformBufferInfo {
        uint32_t set;
        uint32_t binding;
        uint32_t size;
        bool isDynamic;
    };

    // Information about uniform variables within a UBO
    struct VulkanUniformVariableInfo {
        std::string name;
        UniformType type;
        uint32_t size;
        uint32_t offset;
        uint32_t arraySize;
        std::string typeName;
    };

    // Structure to hold all shader resources information
    struct ShaderResourcesInfo {
        std::vector<VulkanDescriptorSetLayoutInfo> descriptorSets;
        VulkanPipelineLayoutInfo pipelineLayout;
        VulkanUniformBufferInfo globalUBO;
        VulkanUniformBufferInfo objectUBO;
        std::vector<VulkanUniformBufferInfo> customUBOs;
        std::vector<VulkanUniformVariableInfo> globalUboVariables;
        std::vector<VulkanUniformVariableInfo> objectUboVariables;
        StageFlags availableStages;
    };

    // Structure to hold shader modules information
    struct ShaderModulesInfo {
        std::vector<std::pair<Stage, std::vector<uint32_t>>> modules;
    };

    // Structure to hold complete shader information
    struct CompleteShaderInfo {
        ShaderResourcesInfo resources;
        ShaderModulesInfo modules;
    };

    // Function declarations
    std::vector<uint32_t> GetSpirvForStage(const std::vector<CompiledStage>& stages, Stage stage);
    std::vector<Stage> GetAvailableStages(const std::vector<CompiledStage>& stages);
    std::vector<VulkanDescriptorSetLayoutInfo> GetDescriptorSetLayoutsInfo(const ShaderMetadata& metadata);
    std::vector<VulkanPushConstantRange> GetPushConstantRanges(const ShaderMetadata& metadata);
    VulkanPipelineLayoutInfo GetPipelineLayoutInfo(const ShaderMetadata& metadata);
    VulkanUniformBufferInfo GetGlobalUboInfo(const ShaderMetadata& metadata);
    VulkanUniformBufferInfo GetObjectUboInfo(const ShaderMetadata& metadata);
    std::vector<VulkanUniformBufferInfo> GetCustomUboInfo(const ShaderMetadata& metadata);
    std::vector<VulkanUniformVariableInfo> GetGlobalUboVariables(const ShaderMetadata& metadata);
    std::vector<VulkanUniformVariableInfo> GetObjectUboVariables(const ShaderMetadata& metadata);
    ShaderResourcesInfo GetShaderResourcesInfo(const ShaderMetadata& metadata);
    ShaderModulesInfo GetShaderModulesInfo(const std::vector<CompiledStage>& stages);
    CompleteShaderInfo GetCompleteShaderInfo(const ShaderData& shaderData, const ShaderMetadata& metadata);

} // namespace ShaderLib