#include "AssetManager.h"
#include <json.hpp>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/fmt/ostr.h>

using json = nlohmann::json;

// Custom formatter for AssetHandle to use in logs
namespace fmt {
    template<>
    struct formatter<AssetHandle> {
        template<typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

        // Add const qualifier here ▼
        template<typename FormatContext>
        auto format(const AssetHandle& handle, FormatContext& ctx) const {
            const char* typeStr = "Unknown";
            switch (handle.type) {
            case AssetType::Mesh: typeStr = "Mesh"; break;
            case AssetType::Texture: typeStr = "Texture"; break;
            case AssetType::Shader: typeStr = "Shader"; break;
            case AssetType::Material: typeStr = "Material"; break;
            default: break;
            }
            return fmt::format_to(ctx.out(), "{}:{}", typeStr, handle.filename);
        }
    };
}

std::string AssetManager::createCacheKey(const AssetHandle& handle) {
    return std::to_string(static_cast<int>(handle.type)) + ":" + handle.filename;
}

AssetManager::AssetManager(
    VramManager& vramManager,
    ShaderModuleManager& shaderManager,
    MaterialManager& materialManager,
    MeshManager& meshManager,
    const std::string& basePath
)
    : m_vramManager(vramManager),
    m_shaderManager(shaderManager),
    m_materialManager(materialManager),
    m_meshManager(meshManager),
    m_assetLoader(basePath) {

    SPDLOG_INFO("AssetManager initialized with base path: {}", basePath);
}

bool AssetManager::ensureLoaded(const AssetHandle handle) {
    if (m_markedForRemoval.count(handle)) {
        m_markedForRemoval.erase(handle);
    }

    // Create a key that includes both filename and type to distinguish assets with same name but different types
    std::string cacheKey = createCacheKey(handle);

    // Check if already in cache
    if (m_assetCache.count(cacheKey) == 0) {
        SPDLOG_INFO("Loading asset: {}", handle);
        try {
            AssetLib::AssetData data = m_assetLoader.load(handle);
            if (data.header.assetType != handle.type) {
                SPDLOG_ERROR("Type mismatch for {}: Expected {}, got {}",
                    handle.filename, static_cast<int>(handle.type), static_cast<int>(data.header.assetType));
                return false; // Type mismatch
            }
            m_assetCache.emplace(cacheKey, std::move(data));

            // If it's a material, cache it immediately
            if (handle.type == AssetLib::AssetType::Material) {
                if (!cacheMaterial(m_assetCache[cacheKey], handle)) {
                    SPDLOG_ERROR("Failed to cache material: {}", handle);
                    return false; // Material caching failed
                }
            }
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("Exception while loading asset {}: {}", handle, e.what());
            return false; // Loading error
        }
        catch (...) {
            SPDLOG_ERROR("Unknown exception while loading asset {}", handle);
            return false; // Loading error
        }
    }

    return true;
}

bool AssetManager::ensureReady(const AssetHandle handle) {
    if (m_markedForRemoval.count(handle)) {
        m_markedForRemoval.erase(handle);
    }

    // Check if already ready - use handle.type explicitly to avoid type confusion
    bool isReady = false;
    switch (handle.type) {
    case AssetLib::AssetType::Mesh:
        isReady = (m_meshHandles.find(handle.filename) != m_meshHandles.end());
        break;
    case AssetLib::AssetType::Texture:
        isReady = (m_vramTextures.find(handle.filename) != m_vramTextures.end());
        break;
    case AssetLib::AssetType::Material:
        isReady = (m_cachedMaterials.find(handle.filename) != m_cachedMaterials.end());
        break;
    case AssetLib::AssetType::Shader:
        isReady = (m_shaders.find(handle.filename) != m_shaders.end());
        break;
    default:
        SPDLOG_ERROR("Unknown asset type for {}", handle);
        return false; // Unknown type
    }

    if (isReady) {
        updateLastUsed(handle);

        // Special handling for materials: if the material is ready, ensure its textures are ready too
        if (handle.type == AssetLib::AssetType::Material) {
            auto it = m_cachedMaterials.find(handle.filename);
            if (it != m_cachedMaterials.end()) {
                MaterialHandle matHandle = it->second;
                Material* material = m_materialManager.get(matHandle);

                if (material) {
                    bool allTexturesReady = true;

                    // Make sure all textures are ready
                    for (auto& param : material->parameters()) {
                        if (auto* textureParam = std::get_if<Material::TextureParam>(&param.value)) {
                            // Ensure the texture is fully ready
                            if (!ensureReady(textureParam->handle)) {
                                SPDLOG_ERROR("Failed to make texture ready for material: {} texture: {}",
                                    handle.filename, textureParam->handle.filename);
                                allTexturesReady = false;
                                continue;
                            }

                            // Update the texture's VRAM handle in the material
                            const VramHandle* vramHandle = getResource<VramHandle>(textureParam->handle);
                            if (vramHandle && vramHandle->isValid()) {
                                textureParam->vramHandle = *vramHandle;
                                SPDLOG_DEBUG("Updated texture parameter with valid VRAM handle: {}",
                                    textureParam->handle.filename);
                            }
                            else {
                                SPDLOG_ERROR("Failed to get valid VRAM handle for texture: {}",
                                    textureParam->handle.filename);
                                allTexturesReady = false;
                            }
                        }
                    }

                    if (!allTexturesReady) {
                        SPDLOG_WARN("Not all textures for material {} are ready", handle.filename);
                        return false;
                    }
                }
            }
        }

        return true;
    }

    // Ensure asset is loaded first
    if (!ensureLoaded(handle)) {
        SPDLOG_ERROR("Failed to load asset {}", handle);
        return false;
    }

    // Prepare for use (upload to GPU as needed)
    if (!prepareForUse(handle)) {
        SPDLOG_ERROR("Failed to prepare asset {} for use", handle);
        return false; // Preparation failed
    }

    updateLastUsed(handle);
    SPDLOG_INFO("Asset {} is now ready", handle);

    // Special handling for materials: make sure their textures are ready too
    if (handle.type == AssetLib::AssetType::Material) {
        auto it = m_cachedMaterials.find(handle.filename);
        if (it != m_cachedMaterials.end()) {
            MaterialHandle matHandle = it->second;
            Material* material = m_materialManager.get(matHandle);

            if (material) {
                bool allTexturesReady = true;

                // Make sure all textures are ready
                for (auto& param : material->parameters()) {
                    if (auto* textureParam = std::get_if<Material::TextureParam>(&param.value)) {
                        // Ensure the texture is fully ready
                        if (!ensureReady(textureParam->handle)) {
                            SPDLOG_ERROR("Failed to make texture ready for material: {} texture: {}",
                                handle.filename, textureParam->handle.filename);
                            allTexturesReady = false;
                            continue;
                        }

                        // Update the texture's VRAM handle in the material
                        const VramHandle* vramHandle = getResource<VramHandle>(textureParam->handle);
                        if (vramHandle && vramHandle->isValid()) {
                            textureParam->vramHandle = *vramHandle;
                            SPDLOG_DEBUG("Updated texture parameter with valid VRAM handle: {}",
                                textureParam->handle.filename);
                        }
                        else {
                            SPDLOG_ERROR("Failed to get valid VRAM handle for texture: {}",
                                textureParam->handle.filename);
                            allTexturesReady = false;
                        }
                    }
                }

                if (!allTexturesReady) {
                    SPDLOG_WARN("Not all textures for material {} are ready", handle.filename);
                    return false;
                }
            }
        }
    }

    return true;
}

void AssetManager::unloadAsset(const AssetHandle& handle) {
    SPDLOG_INFO("Unloading asset: {}", handle);
    unloadAssetInternal(handle);

    // Update last used tracking
    switch (handle.type) {
    case AssetType::Mesh:
        m_meshLastUsed.erase(handle.filename);
        break;
    case AssetType::Texture:
        m_textureLastUsed.erase(handle.filename);
        break;
    case AssetType::Shader:
        m_shaderLastUsed.erase(handle.filename);
        break;
    case AssetType::Material:
        m_materialLastUsed.erase(handle.filename);
        break;
    default:
        SPDLOG_WARN("Unknown asset type for {} in unloadAsset", handle);
        break;
    }
}

void AssetManager::unloadAssetInternal(const AssetHandle& handle) {
    switch (handle.type) {
    case AssetType::Mesh: {
        auto it = m_meshHandles.find(handle.filename);
        if (it != m_meshHandles.end()) {
            m_meshManager.destroyMesh(it->second);
            m_meshHandles.erase(it);
        }
        break;
    }
    case AssetType::Texture: {
        auto it = m_vramTextures.find(handle.filename);
        if (it != m_vramTextures.end()) {
            m_vramManager.freeResource(it->second);
            m_vramTextures.erase(it);
        }
        break;
    }
    case AssetType::Shader: {
        auto it = m_shaders.find(handle.filename);
        if (it != m_shaders.end()) {
            m_shaderManager.destroyShader(it->second);
            m_shaders.erase(it);
        }
        break;
    }
    case AssetType::Material: {
        auto it = m_cachedMaterials.find(handle.filename);
        if (it != m_cachedMaterials.end()) {
            m_materialManager.destroyMaterial(it->second);
            m_cachedMaterials.erase(it);
            m_materialShaderHandles.erase(handle.filename);
        }
        break;
    }
    default:
        SPDLOG_ERROR("Unknown asset type in unloadAssetInternal: {}",
            static_cast<int>(handle.type));
        throw std::runtime_error("Unknown asset type");
    }
}

void AssetManager::purgeUnusedAssets(float vramThresholdPercentage, uint64_t ageThresholdFrames) {
    SPDLOG_INFO("Purging unused assets - VRAM threshold: {}%, age threshold: {} frames",
        vramThresholdPercentage * 100.0f, ageThresholdFrames);

    // Process assets marked for removal
    auto markIt = m_markedForRemoval.begin();
    while (markIt != m_markedForRemoval.end()) {
        const AssetHandle& handle = *markIt;
        unloadAssetInternal(handle);
        m_assetCache.erase(handle.filename);
        markIt = m_markedForRemoval.erase(markIt);
    }

    // Release resources based on age
    auto processAgeBasedUnload = [&](auto& lastUsedMap, AssetType type) {
        std::vector<AssetHandle> toUnload;
        for (const auto& [filename, lastUsed] : lastUsedMap) {
            if (m_currentFrame - lastUsed > ageThresholdFrames) {
                toUnload.emplace_back(type, filename);
            }
        }

        if (!toUnload.empty()) {
            SPDLOG_INFO("Age-based unload: found {} assets of type {} to unload",
                toUnload.size(), static_cast<int>(type));
        }

        for (const auto& handle : toUnload) {
            unloadAsset(handle);
        }
        };

    if (ageThresholdFrames > 0) {
        processAgeBasedUnload(m_meshLastUsed, AssetType::Mesh);
        processAgeBasedUnload(m_textureLastUsed, AssetType::Texture);
        processAgeBasedUnload(m_shaderLastUsed, AssetType::Shader);
        processAgeBasedUnload(m_materialLastUsed, AssetType::Material);
    }

    // Release resources based on VRAM usage
    const float currentUsage = m_vramManager.getVramUsagePercentage();

    if (currentUsage > vramThresholdPercentage) {
        SPDLOG_WARN("VRAM usage ({}%) exceeds threshold ({}%), performing cleanup",
            currentUsage * 100.0f, vramThresholdPercentage * 100.0f);

        struct AssetInfo {
            AssetHandle handle;
            uint64_t size;
            uint64_t lastUsed;
        };

        std::vector<AssetInfo> assets;
        const uint64_t vramBudget = m_vramManager.getVramBudget();
        const uint64_t targetUsage = static_cast<uint64_t>(vramBudget * vramThresholdPercentage);

        // Collect asset information
        auto collectAssets = [&](const auto& container, AssetType type) {
            for (const auto& [filename, _] : container) {
                uint64_t size = 0;
                if constexpr (std::is_same_v<std::decay_t<decltype(container)>, decltype(m_meshHandles)>) {
                    const Mesh* mesh = m_meshManager.getMesh((m_meshHandles.at(filename)));
                    size += m_vramManager.getResourceSize(mesh->vertexBuffer);
                    size += m_vramManager.getResourceSize(mesh->indexBuffer);
                }
                else if constexpr (std::is_same_v<std::decay_t<decltype(container)>, decltype(m_vramTextures)>) {
                    size += m_vramManager.getResourceSize(m_vramTextures.at(filename));
                }

                assets.push_back({
                    AssetHandle(type, filename),
                    size,
                    type == AssetType::Mesh ? m_meshLastUsed[filename] :
                    type == AssetType::Texture ? m_textureLastUsed[filename] : 0
                    });
            }
            };

        collectAssets(m_meshHandles, AssetType::Mesh);
        collectAssets(m_vramTextures, AssetType::Texture);

        // Sort by oldest first
        std::sort(assets.begin(), assets.end(), [](const auto& a, const auto& b) {
            return a.lastUsed < b.lastUsed;
            });

        // Release until target is reached
        uint64_t currentVram = m_vramManager.getVramUsed();
        size_t unloadedCount = 0;
        uint64_t freedMemory = 0;

        for (const auto& asset : assets) {
            if (currentVram <= targetUsage) break;

            uint64_t before = m_vramManager.getVramUsed();
            unloadAsset(asset.handle);
            currentVram = m_vramManager.getVramUsed();

            uint64_t freed = before - currentVram;
            freedMemory += freed;
            unloadedCount++;
        }

        if (unloadedCount > 0) {
            SPDLOG_INFO("VRAM cleanup: unloaded {} assets, freed {} bytes, usage now: {:.2f}%",
                unloadedCount, freedMemory,
                static_cast<float>(m_vramManager.getVramUsed()) / vramBudget * 100.0f);
        }
    }
}

void AssetManager::releaseAsset(const AssetHandle& handle) {
    // Remove from loaded cache
    std::string cacheKey = createCacheKey(handle);
    auto cacheIt = m_assetCache.find(cacheKey);
    if (cacheIt != m_assetCache.end()) {
        m_assetCache.erase(cacheIt);
    }

    // Mark for removal if it's ready
    bool isReady = false;
    switch (handle.type) {
    case AssetType::Mesh: isReady = m_meshHandles.count(handle.filename); break;
    case AssetType::Texture: isReady = m_vramTextures.count(handle.filename); break;
    case AssetType::Shader: isReady = m_shaders.count(handle.filename); break;
    case AssetType::Material: isReady = m_cachedMaterials.count(handle.filename); break;
    default: break;
    }

    if (isReady) {
        SPDLOG_INFO("Asset {} marked for removal", handle);
        m_markedForRemoval.insert(handle);
    }
}

void AssetManager::advanceFrame() {
    ++m_currentFrame;
    purgeUnusedAssets(0.8f, 300);
}

void AssetManager::updateLastUsed(const AssetHandle& handle) {
    switch (handle.type) {
    case AssetLib::AssetType::Mesh:
        m_meshLastUsed[handle.filename] = m_currentFrame;
        break;
    case AssetLib::AssetType::Texture:
        m_textureLastUsed[handle.filename] = m_currentFrame;
        break;
    case AssetLib::AssetType::Shader:
        m_shaderLastUsed[handle.filename] = m_currentFrame;
        break;
    case AssetLib::AssetType::Material:
        m_materialLastUsed[handle.filename] = m_currentFrame;
        break;
    default:
        SPDLOG_WARN("Attempted to update last used for unknown asset type: {}", static_cast<int>(handle.type));
        break;
    }
}

bool AssetManager::prepareForUse(const AssetHandle& handle) {
    std::string cacheKey = createCacheKey(handle);
    auto it = m_assetCache.find(cacheKey);
    if (it == m_assetCache.end()) {
        if (!ensureLoaded(handle)) {
            SPDLOG_ERROR("Failed to load asset: {}", handle.filename);
            return false; // Asset could not be loaded
        }
        it = m_assetCache.find(cacheKey);
        if (it == m_assetCache.end()) {
            SPDLOG_ERROR("Asset should be in cache after loading but was not found: {}", handle);
            return false;
        }
    }

    const AssetLib::AssetData& data = it->second;

    try {
        bool success = false;

        switch (handle.type) {
        case AssetLib::AssetType::Mesh:
            if (m_meshHandles.find(handle.filename) == m_meshHandles.end()) {
                success = uploadMesh(data, handle);
            }
            else {
                success = true;
            }
            break;
        case AssetLib::AssetType::Texture:
            if (m_vramTextures.find(handle.filename) == m_vramTextures.end()) {
                success = uploadTexture(data, handle);
            }
            else {
                success = true;
            }
            break;
        case AssetLib::AssetType::Shader:
            if (m_shaders.find(handle.filename) == m_shaders.end()) {
                success = createShader(data, handle);
            }
            else {
                success = true;
            }
            break;
        case AssetLib::AssetType::Material:
            if (m_cachedMaterials.find(handle.filename) == m_cachedMaterials.end()) {
                success = cacheMaterial(data, handle);
            }
            else {
                success = true;
            }
            break;
        default:
            SPDLOG_ERROR("Unknown asset type: {} for asset {}", static_cast<int>(handle.type), handle);
            return false; // Unknown type
        }

        if (!success) {
            SPDLOG_ERROR("Failed to prepare asset: {}", handle);
            return false;
        }
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during asset preparation: {} for asset {}", e.what(), handle);
        return false; // Error during preparation
    }
    catch (...) {
        SPDLOG_ERROR("Unknown exception during asset preparation for {}", handle);
        return false; // Error during preparation
    }

    return true;
}

bool AssetManager::uploadMesh(const AssetLib::AssetData& data, const AssetHandle& handle) {
    try {
        auto [meshInfo, vertexData, indexData] = AssetLib::ReadMesh(data);

        SPDLOG_INFO("Uploading mesh: {} (vertices: {}, indices: {})",
            handle, meshInfo.vertexCount, meshInfo.indexCount);

        // Create a mesh handle using the mesh manager
        MeshHandle meshHandle = m_meshManager.createMesh(
            meshInfo,
            vertexData,
            indexData
        );

        if (!meshHandle) {
            SPDLOG_ERROR("Mesh manager returned invalid handle for mesh: {}", handle);
            return false;
        }

        m_meshHandles.emplace(handle.filename, meshHandle);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error uploading mesh {}: {}", handle, e.what());
        return false;
    }
    catch (...) {
        SPDLOG_ERROR("Unknown error uploading mesh: {}", handle);
        return false;
    }
}

bool AssetManager::uploadTexture(const AssetLib::AssetData& data, const AssetHandle& handle) {
    try {
        auto [texInfo, decompressedData] = AssetLib::ReadTexture(data);

        // Map texture format
        Graphics::ImageFormat format;
        switch (texInfo.format) {
        case AssetLib::TextureFormat::RGBA8:
            format = Graphics::ImageFormat::R8G8B8A8_UNORM;
            break;
        case AssetLib::TextureFormat::BC7:
            format = Graphics::ImageFormat::BC7_UNORM;
            break;
        default:
            SPDLOG_ERROR("Unsupported texture format: {} for texture {}",
                static_cast<int>(texInfo.format), handle);
            throw std::runtime_error("Unsupported texture format");
        }

        // Image configuration
        Graphics::ImageCreateInfo imageInfo{
            .width = texInfo.width,
            .height = texInfo.height,
            .format = format,
            .usage = Graphics::ImageUsage::TransferDst | Graphics::ImageUsage::Sampled,
            .mipLevels = texInfo.mipLevels,
            .samples = Settings::MsaaSampleCount::Samples1
        };

        SPDLOG_INFO("Uploading texture: {} ({}x{}, {} mip levels)",
            handle, texInfo.width, texInfo.height, texInfo.mipLevels);

        // Create resource in VRAM
        VramHandle vramTexture;
        vramTexture = m_vramManager.createImage(
            imageInfo,
            decompressedData.data()
        );

        if (!vramTexture.isValid()) {
            SPDLOG_ERROR("VRAM manager returned invalid handle for texture: {}", handle);
            return false;
        }

        m_vramTextures.emplace(handle.filename, std::move(vramTexture));
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error uploading texture {}: {}", handle, e.what());
        return false;
    }
    catch (...) {
        SPDLOG_ERROR("Unknown error uploading texture: {}", handle);
        return false;
    }
}

bool AssetManager::createShader(const AssetLib::AssetData& data, const AssetHandle& handle) {
    try {
        auto [metadata, shaderStages] = AssetLib::ReadShader(data);

        SPDLOG_INFO("Creating shader: {} with {} stages", handle, shaderStages.size());

        ShaderHandle shaderHandle = m_shaderManager.createShader(metadata, shaderStages);

        if (!shaderHandle) {
            SPDLOG_ERROR("Shader manager returned invalid handle for shader: {}", handle);
            return false;
        }

        // Store mapping between asset handle and shader handle
        m_shaders.emplace(handle.filename, std::move(shaderHandle));
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error creating shader {}: {}", handle, e.what());
        return false;
    }
    catch (...) {
        SPDLOG_ERROR("Unknown error creating shader: {}", handle);
        return false;
    }
}

bool AssetManager::cacheMaterial(const AssetLib::AssetData& data, const AssetHandle& handle) {
    if (m_cachedMaterials.find(handle.filename) != m_cachedMaterials.end()) {
        return true; // Already cached
    }

    try {
        auto [materialInfo, parameters, parameterData] = AssetLib::ReadMaterial(data);

        // Create shader handle for this material
        std::string shaderName(materialInfo.shaderName.data());
        SPDLOG_INFO("Caching material: {} (using shader: {})", handle, shaderName);

        AssetHandle shaderHandle(AssetType::Shader, shaderName);
        m_materialShaderHandles[handle.filename] = shaderHandle;

        // Ensure the shader is loaded (but not necessarily ready)
        if (!ensureLoaded(shaderHandle)) {
            SPDLOG_ERROR("Failed to load required shader: {} for material: {}", shaderName, handle);
            throw std::runtime_error("Failed to load shader for material: " + handle.filename);
        }

        // Create the shader handle needed for material creation
        ShaderHandle shaderModuleHandle;
        if (ensureReady(shaderHandle)) {
            shaderModuleHandle = *getResource<ShaderHandle>(shaderHandle);
        }
        else {
            prepareForUse(shaderHandle);
            shaderModuleHandle = *getResource<ShaderHandle>(shaderHandle);
        }

        // Create material in material manager
        MaterialHandle matHandle = m_materialManager.createMaterial(
            handle.filename,
            data,
            shaderModuleHandle
        );

        if (!matHandle) {
            SPDLOG_ERROR("Material manager returned invalid handle for material: {}", handle);
            return false;
        }

        // For each texture parameter in the material, ensure it's loaded (but not ready yet)
        Material* material = m_materialManager.get(matHandle);
        if (material) {
            for (auto& param : material->parameters()) {
                if (auto* textureParam = std::get_if<Material::TextureParam>(&param.value)) {
                    // Only ensure the texture is loaded, not ready
                    ensureLoaded(textureParam->handle);
                }
            }
        }

        m_cachedMaterials[handle.filename] = matHandle;
        updateLastUsed(handle);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error caching material {}: {}", handle, e.what());
        return false;
    }
    catch (...) {
        SPDLOG_ERROR("Unknown error caching material: {}", handle);
        return false;
    }
}