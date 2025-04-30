#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include "ShaderModule.h"
#include <string>
#include <AssetLib.h>
#include "UniformBufferManager.h"
#include "DescriptorLayoutManager.h"
#include "PipelineLayoutManager.h"

struct ShaderModuleHandle;

struct CombinedShader {
    std::unordered_map<AssetLib::ShaderStage, ShaderModuleHandle> stages;
};

// New structure to hold the resources for a shader program
struct ShaderResources {
    std::unordered_map<uint32_t, DescriptorLayoutHandle> descriptorLayouts; // Set number -> layout handle
    PipelineLayoutHandle pipelineLayout;
    std::unordered_map<std::string, UniformBufferHandle> uniformBuffers; // Buffer name -> buffer handle
};

struct ShaderProgramHandle {
    uint32_t id;
    constexpr explicit ShaderProgramHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const ShaderProgramHandle&) const = default;
    explicit operator bool() const { return id != 0; }
};

namespace std {
    template<> struct hash<ShaderProgramHandle> {
        size_t operator()(const ShaderProgramHandle& h) const {
            return hash<uint32_t>()(h.id);
        }
    };
}

class ShaderModuleManager {
public:
    ShaderModuleManager(
        const LogicalDevice& device,
        UniformBufferManager& uniformBufferManager,
        DescriptorLayoutManager& descriptorLayoutManager,
        PipelineLayoutManager& pipelineLayoutManager
    );

    // Create a shader module from SPIRV code
    ShaderModuleHandle createFromSPIRV(
        const std::vector<uint32_t>& spirvCode,
        const ShaderReflection& reflection
    );

    // Destroy a shader module
    void destroy(ShaderModuleHandle handle);

    // Get a shader module by handle
    ShaderModule& get(ShaderModuleHandle handle);

    // Check if a shader module handle is valid
    bool isValid(ShaderModuleHandle handle) const;

    // Create a shader program from combined shaders
    ShaderProgramHandle createProgram(
        const CombinedShader& shaders,
        const std::string& debugName = ""
    );

    // Destroy a shader program
    void destroyProgram(ShaderProgramHandle handle);

    // Get shader resources for a program
    const ShaderResources& getProgramResources(ShaderProgramHandle handle) const;

    // Update a uniform buffer field for a program
    void updateUniformField(
        ShaderProgramHandle program,
        const std::string& bufferName,
        const std::string& fieldName,
        const UniformBufferManager::UniformValue& value
    );

    // Update an array uniform buffer field for a program
    void updateUniformArrayField(
        ShaderProgramHandle program,
        const std::string& bufferName,
        const std::string& fieldName,
        const std::vector<UniformBufferManager::UniformValue>& values
    );

    // Apply all pending uniform buffer updates
    void applyUniformUpdates(ShaderProgramHandle program);

private:
    const LogicalDevice& m_device;
    UniformBufferManager& m_uniformBufferManager;
    DescriptorLayoutManager& m_descriptorLayoutManager;
    PipelineLayoutManager& m_pipelineLayoutManager;

    std::unordered_map<ShaderModuleHandle, std::unique_ptr<ShaderModule>> m_modules;
    std::unordered_map<ShaderProgramHandle, ShaderResources> m_programResources;

    uint32_t m_nextModuleHandle = 1;
    uint32_t m_nextProgramHandle = 1;

    // Helper methods
    DescriptorLayoutHandle createDescriptorLayout(
        const std::vector<DescriptorBindingInfo>& bindings,
        VkShaderStageFlags shaderStages
    );

    PipelineLayoutHandle createPipelineLayout(
        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<PushConstantRangeInfo>& pushConstants
    );

    UniformBufferHandle createUniformBuffer(
        const std::vector<DescriptorBindingInfo>& uniformBuffers,
        const std::string& bufferName
    );

    VkShaderStageFlags convertShaderStageFlags(uint8_t flags) const;
    VkDescriptorType convertDescriptorType(DescriptorBindingInfo::Type type) const;

    std::vector<uint32_t> readFile(const std::string& filename) const;
};