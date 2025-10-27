#pragma once

#include "ShaderLib.h"
#include <vector>
#include <utility>
#include <string>
#include <cstdint>

namespace ShaderLib {

    // ============================================================================
    // VULKAN DESCRIPTOR STRUCTURES
    // ============================================================================

    struct VulkanDescriptorBindingInfo {
        uint32_t binding;
        uint32_t descriptorType;
        uint32_t descriptorCount;
        uint32_t stageFlags;
    };

    struct VulkanDescriptorSetLayoutInfo {
        uint32_t setNumber;
        std::vector<VulkanDescriptorBindingInfo> bindings;
    };

    // ============================================================================
    // VULKAN PUSH CONSTANT STRUCTURES
    // ============================================================================

    struct VulkanPushConstantRange {
        uint32_t stageFlags;
        uint32_t offset;
        uint32_t size;
    };

    // ============================================================================
    // VULKAN PIPELINE LAYOUT STRUCTURES
    // ============================================================================

    struct VulkanPipelineLayoutInfo {
        std::vector<uint32_t> setLayoutIndices;
        std::vector<VulkanPushConstantRange> pushConstantRanges;
    };

    // ============================================================================
    // VULKAN BUFFER STRUCTURES
    // ============================================================================

    struct VulkanBufferInfo {
        uint32_t set;
        uint32_t binding;
        uint32_t size;
        BufferType bufferType;
        LayoutStandard layoutStandard;
        bool isDynamic;

        // Helper methods
        bool IsUniformBuffer() const { return bufferType == BufferType::Uniform; }
        bool IsStorageBuffer() const { return bufferType == BufferType::Storage; }
    };

    struct VulkanBufferVariableInfo {
        std::string name;
        BaseType baseType;
        uint32_t size;
        uint32_t offset;

        // For composite types (Struct/Array)
        bool IsComposite() const {
            return baseType == BaseType::Struct || baseType == BaseType::Array;
        }
    };

    // ============================================================================
    // VULKAN SHADER RESOURCES STRUCTURES
    // ============================================================================

    struct ShaderResourcesInfo {
        std::vector<VulkanDescriptorSetLayoutInfo> descriptorSets;
        VulkanPipelineLayoutInfo pipelineLayout;
        VulkanBufferInfo globalUBO;
        VulkanBufferInfo objectUBO;
        std::vector<VulkanBufferInfo> customBuffers;
        std::vector<VulkanBufferVariableInfo> globalUboVariables;
        std::vector<VulkanBufferVariableInfo> objectUboVariables;
        StageFlags availableStages;
    };

    struct ShaderModulesInfo {
        std::vector<std::pair<Stage, std::vector<uint32_t>>> modules;
    };

    struct CompleteShaderInfo {
        ShaderResourcesInfo resources;
        ShaderModulesInfo modules;
    };

    // ============================================================================
    // CONVERSION FUNCTIONS
    // ============================================================================

    // Convert ShaderLib types to Vulkan types
    uint32_t GetVulkanShaderStageFlags(StageFlags stageFlags);
    uint32_t GetVulkanDescriptorType(DescriptorType type);

    // ============================================================================
    // QUERY FUNCTIONS
    // ============================================================================

    // Get SPIR-V for specific stage
    std::vector<uint32_t> GetSpirvForStage(const std::vector<CompiledStage>& stages, Stage stage);

    // Get available stages
    std::vector<Stage> GetAvailableStages(const std::vector<CompiledStage>& stages);

    // ============================================================================
    // DESCRIPTOR AND LAYOUT INFO FUNCTIONS
    // ============================================================================

    std::vector<VulkanDescriptorSetLayoutInfo> GetDescriptorSetLayoutsInfo(const ShaderMetadata& metadata);
    std::vector<VulkanPushConstantRange> GetPushConstantRanges(const ShaderMetadata& metadata);
    VulkanPipelineLayoutInfo GetPipelineLayoutInfo(const ShaderMetadata& metadata);

    // ============================================================================
    // BUFFER INFO FUNCTIONS
    // ============================================================================

    VulkanBufferInfo GetGlobalUboInfo(const ShaderMetadata& metadata);
    VulkanBufferInfo GetObjectUboInfo(const ShaderMetadata& metadata);
    std::vector<VulkanBufferInfo> GetCustomBuffersInfo(const ShaderMetadata& metadata);

    // ============================================================================
    // VARIABLE INFO FUNCTIONS
    // ============================================================================

    std::vector<VulkanBufferVariableInfo> GetBufferVariables(const BufferObject& buffer);
    std::vector<VulkanBufferVariableInfo> GetGlobalUboVariables(const ShaderMetadata& metadata);
    std::vector<VulkanBufferVariableInfo> GetObjectUboVariables(const ShaderMetadata& metadata);

    // ============================================================================
    // COMPLETE INFO FUNCTIONS
    // ============================================================================

    ShaderResourcesInfo GetShaderResourcesInfo(const ShaderMetadata& metadata);
    ShaderModulesInfo GetShaderModulesInfo(const std::vector<CompiledStage>& stages);
    CompleteShaderInfo GetCompleteShaderInfo(const ShaderData& shaderData, const ShaderMetadata& metadata);

} // namespace ShaderLib