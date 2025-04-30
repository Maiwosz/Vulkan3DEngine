#pragma once
#include "AssetLoader.h"
#include "AssetHandle.h"
#include "VramManager.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include "VramAssetTypes.h"
#include <type_traits>
#include "ShaderModuleManager.h"
#include <unordered_set>
#include "MaterialManager.h"
#include "Paths.h"


class AssetManager {
public:
    explicit AssetManager(
        VramManager& vramManager,
        ShaderModuleManager& shaderManager,
        MaterialManager& materialManager,
        const std::string& basePath = ASSETS_COMP
    );

    bool ensureLoaded(const AssetHandle handle);
    bool ensureReady(const AssetHandle handle);
    void unloadAsset(const AssetHandle& handle);
    void purgeUnusedAssets(float vramThresholdPercentage, uint64_t ageThresholdFrames);
    void releaseAsset(const AssetHandle& handle);

    void advanceFrame();

    template<typename T>
    auto getResource(const AssetHandle& handle) const {
        static_assert(std::is_same_v<T, VramMesh> ||
            std::is_same_v<T, VramTexture> ||
            std::is_same_v<T, CombinedShader> ||
            std::is_same_v<T, Material>,
            "Unsupported resource type. Valid types: VramMesh, VramTexture, CombinedShader, MaterialData");

        if constexpr (std::is_same_v<T, VramMesh>) {
            if (handle.type != AssetType::Mesh) return static_cast<const VramMesh*>(nullptr);
            auto it = m_vramMeshes.find(handle.filename);
            return it != m_vramMeshes.end() ? &it->second : nullptr;
        }
        else if constexpr (std::is_same_v<T, VramTexture>) {
            if (handle.type != AssetType::Texture) return static_cast<const VramTexture*>(nullptr);
            auto it = m_vramTextures.find(handle.filename);
            return it != m_vramTextures.end() ? &it->second : nullptr;
        }
        else if constexpr (std::is_same_v<T, CombinedShader>) {
            if (handle.type != AssetType::Shader) return static_cast<const CombinedShader*>(nullptr);
            auto it = m_combinedShaders.find(handle.filename);
            return it != m_combinedShaders.end() ? &it->second : nullptr;
        }
        else if constexpr (std::is_same_v<T, Material>) {
            if (handle.type != AssetType::Material) return MaterialHandle{ 0 };
            auto it = m_cachedMaterials.find(handle.filename);
            return it != m_cachedMaterials.end() ? it->second : MaterialHandle{ 0 };
        }
    }
private:
    VramManager& m_vramManager;
    ShaderModuleManager& m_shaderManager;
    MaterialManager& m_materialManager;
    AssetLoader m_assetLoader;

    std::unordered_map<std::string, AssetLib::AssetData> m_assetCache;
    std::unordered_map<std::string, VramMesh> m_vramMeshes;
    std::unordered_map<std::string, VramTexture> m_vramTextures;
    std::unordered_map<std::string, CombinedShader> m_combinedShaders;
    std::unordered_map<std::string, MaterialHandle> m_cachedMaterials;
    std::unordered_map<std::string, AssetHandle> m_materialShaderHandles;

    std::unordered_set<AssetHandle> m_markedForRemoval;

    uint64_t m_currentFrame = 0;
    std::unordered_map<std::string, uint64_t> m_meshLastUsed;
    std::unordered_map<std::string, uint64_t> m_textureLastUsed;
    std::unordered_map<std::string, uint64_t> m_shaderLastUsed;
    std::unordered_map<std::string, uint64_t> m_materialLastUsed;

    bool prepareForUse(const AssetHandle& handle);
    void uploadMesh(const AssetLib::AssetData& data, const AssetHandle& handle);
    void uploadTexture(const AssetLib::AssetData& data, const AssetHandle& handle);
    void createShader(const AssetLib::AssetData& data, const AssetHandle& handle);
    void cacheMaterial(const AssetLib::AssetData& data, const AssetHandle& handle);

    void unloadAssetInternal(const AssetHandle& handle);

    ShaderReflection parseShaderMetadata(const std::vector<uint8_t>& metadata, uint32_t spirvVersion);

    void updateLastUsed(const AssetHandle& handle);
};