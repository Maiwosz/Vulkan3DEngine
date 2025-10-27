#pragma once
#include "Material.h"
#include "Handle.h"
#include "ShaderManager.h"
#include "VramManager.h"
#include "ImageSamplerManager.h"
#include "BufferManager.h"
#include "DescriptorAllocator.h"
#include "DescriptorWriter.h"
#include "TextureManager.h"
#include <memory>
#include <vector>
#include <unordered_map>

class MaterialResourceFactory {
public:
    MaterialResourceFactory(
        const LogicalDevice& device,
        ShaderManager& shaderManager,
        ImageSamplerManager& samplerManager,
        BufferManager& uniformBufferManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager,
        TextureManager& textureManager
    );

    ~MaterialResourceFactory() = default;

    // Create new material descriptor set (multiple UBOs + textures)
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> createMaterialDescriptorSet(
        ShaderHandle shaderHandle,
        const std::vector<Material::Parameter>& parameters
    );

private:
    // Structure to hold created UBO handles with their binding info
    struct MaterialUBO {
        uint32_t binding;
        SmartHandle<BufferHandle, Buffer> bufferHandle;
        const ShaderLib::BufferObject* uboInfo;
    };

    // Create all UBOs required by the shader for material parameters
    std::vector<MaterialUBO> createMaterialUniformBuffers(
        ShaderHandle shaderHandle,
        const std::vector<Material::Parameter>& parameters
    );

    // Create descriptor set with multiple UBOs and textures
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> createDescriptorSetInternal(
        ShaderHandle shaderHandle,
        const std::vector<MaterialUBO>& ubos,
        const std::vector<Material::Parameter>& parameters
    );

    // Update single UBO with matching parameters
    void updateUniformBufferFromParameters(
        const SmartHandle<BufferHandle, Buffer>& uboHandle,
        const ShaderLib::BufferObject& uboInfo,
        const std::vector<Material::Parameter>& parameters
    );

    // Helper: Find parameters that belong to a specific UBO
    std::vector<const Material::Parameter*> findParametersForUBO(
        const ShaderLib::BufferObject& uboInfo,
        const std::vector<Material::Parameter>& parameters
    ) const;

    const LogicalDevice& m_device;
    ShaderManager& m_shaderManager;
    ImageSamplerManager& m_samplerManager;
    DescriptorLayoutManager& m_descriptorLayoutManager;
    BufferManager& m_bufferManager;
    DescriptorAllocator& m_descriptorAllocator;
    TextureManager& m_textureManager;
};