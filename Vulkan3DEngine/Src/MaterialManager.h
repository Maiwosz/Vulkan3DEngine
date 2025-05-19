#pragma once
#include "Material.h"
#include "Handle.h"
#include "ShaderManager.h"
#include "VramManager.h"
#include "AssetLib.h"
#include "Settings.h"
#include "ImageSamplerManager.h"
#include "IAssetHandler.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include "MaterialResourceManager.h"
#include "TextureManager.h"

class MaterialManager : public IAssetHandler {
public:
    MaterialManager(
        const LogicalDevice& device,
        ShaderManager& shaderManager,
        ImageSamplerManager& samplerManager,
        UniformBufferManager& uniformBufferManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager,
        TextureManager& textureManager
    );
    ~MaterialManager();

    // IAssetHandler implementation
    bool prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) override;
    void unloadAsset(const std::string& filename) override;
    bool isAssetReady(const std::string& filename) const override;
    uint64_t getAssetSize(const std::string& filename) const override;
    bool isInVram() const override { return true; } // Materials use VRAM for descriptors and UBOs
    std::vector<AssetDependency> getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const override;
    std::any getResourceInternal(const AssetHandle& handle) const override;
    std::any getHandleInternal(const std::string& filename) const override;

    // Additional public interface for MaterialHandle-based access
    Material* getMaterial(MaterialHandle handle);
    const Material* getMaterial(MaterialHandle handle) const;
    VkDescriptorSet getMaterialDescriptorSet(MaterialHandle handle);

    // Convert AssetLib parameter to Material parameter
    Material::ParamValue convertParameter(
        const AssetLib::MaterialParameter& assetParam,
        const std::vector<uint8_t>& parameterData,
        uint32_t dataOffset
    );

private:
    struct MaterialData {
        std::unique_ptr<Material> material;
        MaterialResourceManager::MaterialResources resources;
        uint64_t estimatedSize;
        bool isReady;
    };

    MaterialHandle createMaterial(
        const AssetHandle& assetHandle,
        const AssetLib::AssetData& assetData,
        ShaderHandle shaderHandle
    );

    Material::Parameter createMaterialParameter(
        const AssetLib::MaterialParameter& assetParam,
        const std::vector<uint8_t>& parameterData,
        ShaderHandle shaderHandle
    );

    uint32_t findBindingForParameter(
        ShaderHandle shaderHandle,
        const std::string& paramName,
        ShaderLib::DescriptorType descriptorType
    );

    MaterialResourceManager::MaterialResources* getOrCreateMaterialResources(MaterialHandle handle);
    void updateTextureHandles(MaterialHandle materialHandle, AssetManager& manager);

    const LogicalDevice& m_device;
    ShaderManager& m_shaderManager;
    ImageSamplerManager& m_samplerManager;
    TextureManager& m_textureManager;

    // Resource manager for UBOs and descriptor sets
    std::unique_ptr<MaterialResourceManager> m_resourceManager;

    // Material storage using MaterialHandle as key
    std::unordered_map<MaterialHandle, MaterialData> m_materials;

    // Mapping from filename to MaterialHandle
    std::unordered_map<std::string, MaterialHandle> m_filenameToHandle;

    uint32_t m_nextHandle = 1;
};