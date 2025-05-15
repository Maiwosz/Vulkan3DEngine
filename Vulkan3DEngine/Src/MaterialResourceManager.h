#pragma once
#include "Material.h"
#include "Handle.h"
#include "ShaderModuleManager.h"
#include "VramManager.h"
#include "ImageSamplerManager.h"
#include "UniformBufferManager.h"
#include "DescriptorAllocator.h"
#include "DescriptorWriter.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

// This class is responsible for creating and managing material resources,
// specifically UBOs and descriptor sets
class MaterialResourceManager {
public:
    struct MaterialResources {
        UniformBufferHandle uniformBuffer; // UBO handle
        VkDescriptorSet descriptorSet;     // Material's descriptor set (set 2)
    };

    MaterialResourceManager(
        ShaderModuleManager& shaderModuleManager,
        ImageSamplerManager& samplerManager,
        UniformBufferManager& uniformBufferManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager,
        VramManager& vramManager,
        const LogicalDevice& device
    );
    ~MaterialResourceManager();

    MaterialResources createMaterialResources(
        ShaderHandle shaderHandle,
        const std::vector<Material::Parameter>& parameters
    );

    void destroyMaterialResources(const MaterialResources& resources);

    bool updateTextureBinding(
        const MaterialResources& resources,
        ShaderHandle shaderHandle,
        const std::string& paramName,
        int paramBinding,
        VramHandle textureHandle,
        VkSampler sampler
    );

    bool updateUniformParameter(
        const MaterialResources& resources,
        ShaderHandle shaderHandle,
        const std::string& paramName,
        const Material::ParamValue& value
    );

    const UniformBufferManager& getUniformBufferManager() const { return m_uniformBufferManager; }

private:
    UniformBufferHandle createMaterialUniformBuffer(
        ShaderHandle shaderHandle,
        const std::vector<Material::Parameter>& parameters
    );

    VkDescriptorSet createMaterialDescriptorSet(
        ShaderHandle shaderHandle,
        UniformBufferHandle uboHandle,
        const std::vector<Material::Parameter>& parameters
    );

    void updateUniformBufferFromParameters(
        UniformBufferHandle uboHandle,
        const ShaderLib::UniformBufferObject& uboInfo,
        const std::vector<Material::Parameter>& parameters
    );

    ShaderModuleManager& m_shaderModuleManager;
    ImageSamplerManager& m_samplerManager;
    DescriptorLayoutManager& m_descriptorLayoutManager;
    UniformBufferManager& m_uniformBufferManager;
    DescriptorAllocator& m_descriptorAllocator;
    const LogicalDevice& m_device;
    VramManager& m_vramManager;
};