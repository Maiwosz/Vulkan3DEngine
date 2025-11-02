#pragma once
#include "Material.h"
#include "ShaderManager.h"
#include "BufferManager.h"
#include "ImageSamplerManager.h"
#include "TextureManager.h"
#include "DescriptorAllocator.h"
#include "DescriptorLayoutManager.h"
#include "AssetLib.h"
#include <memory>
#include <string>
#include <vector>

class MaterialFactory {
public:
    MaterialFactory(
        const LogicalDevice& device,
        ShaderManager& shaderManager,
        BufferManager& bufferManager,
        ImageSamplerManager& samplerManager,
        TextureManager& textureManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager
    );

    // Create material with default parameters from shader
    std::unique_ptr<Material> createMaterial(
        const std::string& name,
        ShaderHandle shaderHandle
    );

    // Create material with custom parameters
    std::unique_ptr<Material> createMaterial(
        const std::string& name,
        ShaderHandle shaderHandle,
        const std::vector<Material::Parameter>& parameters
    );

    // Create material from asset definition
    std::unique_ptr<Material> createMaterialFromAsset(
        const std::string& name,
        ShaderHandle shaderHandle,
        const AssetLib::MaterialDefinition& materialDef,
        AssetManager& assetManager
    );

    // Helper: Create parameter from asset definition
    Material::Parameter createParameterFromAsset(
        const AssetLib::ParameterValue& assetParam,
        ShaderHandle shaderHandle,
        AssetManager& assetManager
    );

private:
    // Generate default parameters from shader metadata
    std::vector<Material::Parameter> createDefaultParameters(ShaderHandle shaderHandle);

    // Find binding for parameter in shader metadata
    uint32_t findBindingForParameter(
        ShaderHandle shaderHandle,
        const std::string& paramName,
        ShaderLib::DescriptorType descriptorType
    );

    // Convert asset parameter to material parameter value
    Material::ParamValue convertParameterValue(
        const AssetLib::ParameterValue& assetParam,
        AssetManager& assetManager
    );

    // Create smart shader handle
    SmartAssetHandle<ShaderHandle, ShaderAsset> createSmartShaderHandle(ShaderHandle shaderHandle);

    // Dependencies
    const LogicalDevice& m_device;
    ShaderManager& m_shaderManager;
    BufferManager& m_bufferManager;
    ImageSamplerManager& m_samplerManager;
    TextureManager& m_textureManager;
    DescriptorAllocator& m_descriptorAllocator;
    DescriptorLayoutManager& m_descriptorLayoutManager;
};