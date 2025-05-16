#pragma once
#include "Material.h"
#include "Handle.h"
#include "ShaderModuleManager.h"
#include "VramManager.h"
#include "AssetLib.h"
#include "Settings.h"
#include "ImageSamplerManager.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include "MaterialResourceManager.h"
#include "TextureManager.h"

class MaterialManager {
public:
    MaterialManager(
        const LogicalDevice& device,
        ShaderModuleManager& shaderModuleManager,
        ImageSamplerManager& samplerManager,
        UniformBufferManager& uniformBufferManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorlayoutManager,
		TextureManager& textureManager
    );
    ~MaterialManager();

    MaterialHandle createMaterial(
        const std::string& name,
        const AssetLib::AssetData& assetData,
        ShaderHandle shaderHandle
    );
    void destroyMaterial(MaterialHandle handle);
    Material* get(MaterialHandle handle);
    const Material* get(MaterialHandle handle) const;

    bool isValid(MaterialHandle handle) const;

    // Convert AssetLib parameter to Material parameter
    Material::ParamValue convertParameter(
        const AssetLib::MaterialParameter& assetParam,
        const std::vector<uint8_t>& parameterData,
        uint32_t dataOffset
    );

    VkDescriptorSet getMaterialDescriptorSet(MaterialHandle handle);

    MaterialResourceManager::MaterialResources* getOrCreateMaterialResources(MaterialHandle handle);

private:
    Material::Parameter createMaterialParameter(
        const AssetLib::MaterialParameter& assetParam,
        const std::vector<uint8_t>& parameterData,
        ShaderHandle shaderHandle
    );
    void convertTextureParameter(
        Material::Parameter& param,
        const AssetLib::MaterialParameter& assetParam,
        const std::vector<uint8_t>& parameterData
    );

    uint32_t findBindingForParameter(
        ShaderHandle shaderHandle,
        const std::string& paramName,
        ShaderLib::DescriptorType descriptorType
    );

	const LogicalDevice& m_device;
    ShaderModuleManager& m_shaderModuleManager;
    ImageSamplerManager& m_samplerManager;
	TextureManager& m_textureManager;

    // Resource manager for UBOs and descriptor sets
    std::unique_ptr<MaterialResourceManager> m_resourceManager;

    // Material storage
    std::unordered_map<MaterialHandle, MaterialResourceManager::MaterialResources> m_materialResources;
    std::unordered_map<MaterialHandle, std::unique_ptr<Material>> m_materials;

    uint32_t m_nextHandle = 1;
};