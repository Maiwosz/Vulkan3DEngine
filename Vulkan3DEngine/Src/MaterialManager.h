#pragma once
#include "Material.h"
#include "MaterialHandle.h"
#include "ShaderModuleManager.h"
#include "VramManager.h"
#include "AssetLib.h"
#include "Settings.h"
#include "ImageSamplerManager.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

class MaterialManager {
public:
    MaterialManager(
        ShaderModuleManager& shaderModuleManager,
        ImageSamplerManager& samplerManager
    );
    ~MaterialManager();

    // Create a material from AssetLib data
    MaterialHandle createMaterial(
        const std::string& name,
        const AssetLib::AssetData& assetData,
        ShaderHandle shaderHandle
    );

    // Destroy a material
    void destroyMaterial(MaterialHandle handle);

    // Get a material by handle
    Material* get(MaterialHandle handle);
    const Material* get(MaterialHandle handle) const;

    // Check if a material handle is valid
    bool isValid(MaterialHandle handle) const;

    // Set material parameter by name
    bool setParameter(MaterialHandle handle, const std::string& paramName, const Material::ParamValue& value);

    // Update material parameters to the GPU
    void updateMaterialParameters(MaterialHandle handle);

    // Convert AssetLib parameter to Material parameter
    Material::ParamValue convertParameter(
        const AssetLib::MaterialParameter& assetParam,
        const std::vector<uint8_t>& parameterData,
        uint32_t dataOffset
    );

private:
    // Helper method to create a material parameter from AssetLib data
    Material::Parameter createMaterialParameter(
        const AssetLib::MaterialParameter& assetParam,
        const std::vector<uint8_t>& parameterData
    );

    void convertTextureParameter(
        Material::Parameter& param,
        const AssetLib::MaterialParameter& assetParam,
        const std::vector<uint8_t>& parameterData
    );

    // Handle counter for uniquely identifying materials
    uint32_t m_nextHandle = 1;

    // Map of materials by handle
    std::unordered_map<MaterialHandle, std::unique_ptr<Material>> m_materials;

    // Reference to the shader module manager
    ShaderModuleManager& m_shaderModuleManager;

    // Reference to sampler manager for texture sampler handling
    ImageSamplerManager& m_samplerManager;
};
