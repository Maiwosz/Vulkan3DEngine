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
#include "MaterialResourceFactory.h"
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
    bool isInVram() const override { return true; }
    std::vector<AssetDependency> getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const override;
    std::any getResourceInternal(const AssetHandle& handle) const override;
    std::any getHandleInternal(const std::string& filename) const override;

    // Public interface for MaterialHandle-based access
    Material* getMaterial(MaterialHandle handle);
    const Material* getMaterial(MaterialHandle handle) const;

    // Parameter modification methods - automatically invalidate descriptor sets
    bool setMaterialParameter(MaterialHandle handle, const std::string& paramName, const Material::ParamValue& value);
    bool getMaterialParameter(MaterialHandle handle, const std::string& paramName, Material::ParamValue& outValue) const;

    // Main descriptor set access method - creates on demand and manages cache
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> getDescriptorSet(MaterialHandle handle);

    // Invalidate descriptor set when material parameters change
    void invalidateDescriptorSet(MaterialHandle handle);

    // Template method for getting handles (needed for AssetManager integration)
    template<typename T>
    T getHandle(const std::string& filename) const {
        auto result = getHandleInternal(filename);
        if (result.has_value()) {
            try {
                return std::any_cast<T>(result);
            }
            catch (const std::bad_any_cast&) {
                return T(); // Return invalid handle on cast failure
            }
        }
        return T(); // Return invalid handle if not found
    }

private:
    struct MaterialData {
        std::unique_ptr<Material> material;
        uint64_t estimatedSize;
        bool isReady;

        // Descriptor set cache - created on demand
        SmartHandle<DescriptorSetHandle, VkDescriptorSet> descriptorSet;
        bool descriptorSetValid;

        // Track sampler handles for dirty checking
        std::vector<SamplerHandle> samplerHandles;

        MaterialData() : estimatedSize(0), isReady(false), descriptorSetValid(false) {}
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

    void updateTextureHandles(MaterialHandle materialHandle, AssetManager& manager);
    void collectSamplerHandles(MaterialHandle materialHandle);

    // Convert AssetLib parameter to Material parameter
    Material::ParamValue convertParameter(
        const AssetLib::MaterialParameter& assetParam,
        const std::vector<uint8_t>& parameterData,
        uint32_t dataOffset
    );

    const LogicalDevice& m_device;
    ShaderManager& m_shaderManager;
    ImageSamplerManager& m_samplerManager;
    TextureManager& m_textureManager;

    // Resource factory for creating descriptor sets
    std::unique_ptr<MaterialResourceFactory> m_resourceFactory;

    // Material storage using MaterialHandle as key
    std::unordered_map<MaterialHandle, MaterialData> m_materials;

    // Mapping from filename to MaterialHandle
    std::unordered_map<std::string, MaterialHandle> m_filenameToHandle;

    uint32_t m_nextHandle = 1;
};