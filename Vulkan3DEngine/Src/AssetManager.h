#pragma once
#include "AssetLoader.h"
#include "AssetHandle.h"
#include "VramManager.h"
#include "TextureManager.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include "MeshManager.h" 
#include <type_traits>
#include "ShaderModuleManager.h"
#include <unordered_set>
#include "MaterialManager.h"
#include "Paths.h"
#include <spdlog/spdlog.h>

class AssetManager {
public:
    explicit AssetManager(
        VramManager& vramManager,
        ShaderModuleManager& shaderManager,
        MaterialManager& materialManager,
        MeshManager& meshManager,
        TextureManager& textureManager,
        const std::string& basePath = ASSETS_COMP
    );
    //~AssetManager();

    bool ensureLoaded(const AssetHandle handle);
    bool ensureReady(const AssetHandle handle);
    void unloadAsset(const AssetHandle& handle);
    void purgeUnusedAssets(float vramThresholdPercentage, uint64_t ageThresholdFrames);
    void releaseAsset(const AssetHandle& handle);

    void advanceFrame();

    template<typename T>
    auto getResource(const AssetHandle& handle) const {
        static_assert(std::is_same_v<T, MeshHandle> ||
            std::is_same_v<T, TextureHandle> ||
            std::is_same_v<T, ShaderHandle> ||
            std::is_same_v<T, Material>,
            "Unsupported resource type. Valid types: MeshHandle, TextureHandle, ShaderHandle, Material");

        // Add detailed debugging for resource retrieval attempts
        SPDLOG_DEBUG("Attempting to get resource of type {} for asset '{}'",
            typeid(T).name(), handle.filename);

        if constexpr (std::is_same_v<T, MeshHandle>) {
            if (handle.type != AssetType::Mesh) {
                SPDLOG_WARN("Type mismatch: Asset '{}' is not a mesh (type {})",
                    handle.filename, static_cast<int>(handle.type));
                return MeshHandle{ 0 };
            }
            auto it = m_meshHandles.find(handle.filename);
            if (it == m_meshHandles.end()) {
                SPDLOG_WARN("Resource not found: Mesh '{}' not in cache", handle.filename);
                return MeshHandle{ 0 };
            }
            SPDLOG_DEBUG("Retrieved mesh handle for '{}'", handle.filename);
            return it->second;
        }
        else if constexpr (std::is_same_v<T, TextureHandle>) {
            if (handle.type != AssetType::Texture) {
                SPDLOG_WARN("Type mismatch: Asset '{}' is not a texture (type {})",
                    handle.filename, static_cast<int>(handle.type));
                return TextureHandle{ 0 };
            }
            auto it = m_textureHandles.find(handle.filename);
            if (it == m_textureHandles.end()) {
                SPDLOG_WARN("Resource not found: Texture '{}' not in cache", handle.filename);
                return TextureHandle{ 0 };
            }
            SPDLOG_DEBUG("Retrieved texture handle for '{}'", handle.filename);
            return it->second;
        }
        else if constexpr (std::is_same_v<T, ShaderHandle>) {
            if (handle.type != AssetType::Shader) {
                SPDLOG_WARN("Type mismatch: Asset '{}' is not a shader (type {})",
                    handle.filename, static_cast<int>(handle.type));
                return static_cast<const ShaderHandle*>(nullptr);
            }
            auto it = m_shaders.find(handle.filename);
            if (it == m_shaders.end()) {
                SPDLOG_WARN("Resource not found: Shader '{}' not in cache", handle.filename);
                return static_cast<const ShaderHandle*>(nullptr);
            }
            SPDLOG_DEBUG("Retrieved shader handle for '{}'", handle.filename);
            return &it->second;
        }
        else if constexpr (std::is_same_v<T, Material>) {
            if (handle.type != AssetType::Material) {
                SPDLOG_WARN("Type mismatch: Asset '{}' is not a material (type {})",
                    handle.filename, static_cast<int>(handle.type));
                return MaterialHandle{ 0 };
            }
            auto it = m_cachedMaterials.find(handle.filename);
            if (it == m_cachedMaterials.end()) {
                SPDLOG_WARN("Resource not found: Material '{}' not in cache", handle.filename);
                return MaterialHandle{ 0 };
            }
            SPDLOG_DEBUG("Retrieved material handle for '{}'", handle.filename);
            return it->second;
        }
    }

private:
    VramManager& m_vramManager;
    ShaderModuleManager& m_shaderManager;
    MaterialManager& m_materialManager;
    MeshManager& m_meshManager;
    TextureManager& m_textureManager;
    AssetLoader m_assetLoader;

    std::unordered_map<std::string, AssetLib::AssetData> m_assetCache;
    std::unordered_map<std::string, MeshHandle> m_meshHandles;
    std::unordered_map<std::string, TextureHandle> m_textureHandles;
    std::unordered_map<std::string, ShaderHandle> m_shaders;
    std::unordered_map<std::string, MaterialHandle> m_cachedMaterials;
    std::unordered_map<std::string, AssetHandle> m_materialShaderHandles;

    std::unordered_set<AssetHandle> m_markedForRemoval;

    uint64_t m_currentFrame = 0;
    std::unordered_map<std::string, uint64_t> m_meshLastUsed;
    std::unordered_map<std::string, uint64_t> m_textureLastUsed;
    std::unordered_map<std::string, uint64_t> m_shaderLastUsed;
    std::unordered_map<std::string, uint64_t> m_materialLastUsed;

    bool prepareForUse(const AssetHandle& handle);
    bool uploadMesh(const AssetLib::AssetData& data, const AssetHandle& handle);
    bool uploadTexture(const AssetLib::AssetData& data, const AssetHandle& handle);
    bool createShader(const AssetLib::AssetData& data, const AssetHandle& handle);
    bool cacheMaterial(const AssetLib::AssetData& data, const AssetHandle& handle);

    void unloadAssetInternal(const AssetHandle& handle);
    void updateLastUsed(const AssetHandle& handle);

    // Helper method to create a unique cache key from an AssetHandle
    std::string createCacheKey(const AssetHandle& handle);
};