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
#include "TextureManager.h"

class MaterialResourceManager {
public:
    struct MaterialResources {
        UniformBufferHandle uniformBuffer; // UBO handle
        VkDescriptorSet descriptorSet;     // Material's descriptor set (set 2)
    };

    MaterialResourceManager(
        const LogicalDevice& device,
        ShaderModuleManager& shaderModuleManager,
        ImageSamplerManager& samplerManager,
        UniformBufferManager& uniformBufferManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager,
		TextureManager& textureManager
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
        TextureHandle textureHandle,
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

    const LogicalDevice& m_device;
    ShaderModuleManager& m_shaderModuleManager;
    ImageSamplerManager& m_samplerManager;
    DescriptorLayoutManager& m_descriptorLayoutManager;
    UniformBufferManager& m_uniformBufferManager;
    DescriptorAllocator& m_descriptorAllocator;
    TextureManager& m_textureManager;
};