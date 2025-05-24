#pragma once
#include "Material.h"
#include "Handle.h"
#include "ShaderManager.h"
#include "VramManager.h"
#include "ImageSamplerManager.h"
#include "UniformBufferManager.h"
#include "DescriptorAllocator.h"
#include "DescriptorWriter.h"
#include "TextureManager.h"
#include <memory>

class MaterialResourceFactory {
public:
    MaterialResourceFactory(
        const LogicalDevice& device,
        ShaderManager& shaderManager,
        ImageSamplerManager& samplerManager,
        UniformBufferManager& uniformBufferManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager,
        TextureManager& textureManager
    );

    ~MaterialResourceFactory() = default;

    // Create new material descriptor set (UBO + textures)
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> createMaterialDescriptorSet(
        ShaderHandle shaderHandle,
        const std::vector<Material::Parameter>& parameters
    );

private:
    // Create UBO for material parameters
    SmartHandle<UniformBufferHandle, Buffer> createMaterialUniformBuffer(
        ShaderHandle shaderHandle,
        const std::vector<Material::Parameter>& parameters
    );

    // Create descriptor set with UBO and textures
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> createDescriptorSetInternal(
        ShaderHandle shaderHandle,
        const SmartHandle<UniformBufferHandle, Buffer>& uboHandle,
        const std::vector<Material::Parameter>& parameters
    );

    // Update UBO with parameter values
    void updateUniformBufferFromParameters(
        const SmartHandle<UniformBufferHandle, Buffer>& uboHandle,
        const ShaderLib::UniformBufferObject& uboInfo,
        const std::vector<Material::Parameter>& parameters
    );

    const LogicalDevice& m_device;
    ShaderManager& m_shaderManager;
    ImageSamplerManager& m_samplerManager;
    DescriptorLayoutManager& m_descriptorLayoutManager;
    UniformBufferManager& m_uniformBufferManager;
    DescriptorAllocator& m_descriptorAllocator;
    TextureManager& m_textureManager;
};