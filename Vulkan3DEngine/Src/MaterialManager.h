#pragma once
#include "Material.h"
#include "MaterialFactory.h"
#include "Handle.h"
#include "ShaderManager.h"
#include "VramManager.h"
#include "AssetLib.h"
#include "Settings.h"
#include "ImageSamplerManager.h"
#include "IAssetHandler.h"
#include "TextureManager.h"
#include "BufferManager.h"
#include "DescriptorAllocator.h"
#include "DescriptorLayoutManager.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <MaterialTypes.h>

class MaterialManager : public ISmartAssetHandler<MaterialHandle, Material> {
public:
    MaterialManager(
        const LogicalDevice& device,
        ShaderManager& shaderManager,
        ImageSamplerManager& samplerManager,
        BufferManager& uniformBufferManager,
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

    // ISmartAssetHandler implementation
    Material* getResource(MaterialHandle handle) const override;
    bool isAssetReady(MaterialHandle handle) const override;

    // Public interface for MaterialHandle-based access
    Material* getMaterial(MaterialHandle handle);
    const Material* getMaterial(MaterialHandle handle) const;

    // Register runtime material (not from asset)
    MaterialHandle registerMaterial(std::unique_ptr<Material> material, const std::string& name);

    // Create a compute material from shader handle
    SmartAssetHandle<MaterialHandle, Material> createComputeMaterial(
        const std::string& shaderName
    );

    // Access to factory for advanced runtime material creation
    MaterialFactory& factory() { return m_factory; }

    // Template method for getting handles (needed for AssetManager integration)
    template<typename T>
    T getHandle(const std::string& filename) const {
        auto result = getHandleInternal(filename);
        if (result.has_value()) {
            try {
                return std::any_cast<T>(result);
            }
            catch (const std::bad_any_cast&) {
                return T();
            }
        }
        return T();
    }

private:
    struct MaterialData {
        std::unique_ptr<Material> material;
        uint64_t estimatedSize;
        bool isReady;
        bool isFromAsset;

        MaterialData() : estimatedSize(0), isReady(false), isFromAsset(true) {}
    };

    // Asset loading
    MaterialHandle loadMaterialFromAsset(
        const AssetHandle& assetHandle,
        const AssetLib::MaterialDefinition& materialDef,
        ShaderHandle shaderHandle,
        AssetManager& manager
    );

    // Texture dependency management
    void updateTextureHandles(MaterialHandle materialHandle, AssetManager& manager);

    // Material factory
    MaterialFactory m_factory;

    // Dependencies (non-owning references)
    ShaderManager& m_shaderManager;
    TextureManager& m_textureManager;

    // Material storage
    std::unordered_map<MaterialHandle, MaterialData> m_materials;
    std::unordered_map<std::string, MaterialHandle> m_filenameToHandle;

    uint32_t m_nextHandle = 1;
};