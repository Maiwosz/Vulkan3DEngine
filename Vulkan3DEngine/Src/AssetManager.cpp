#include "AssetManager.h"
#include <json.hpp>
#include <iostream>
using json = nlohmann::json;

AssetManager::AssetManager(
    VramManager& vramManager,
    ShaderModuleManager& shaderManager,
    MaterialManager& materialManager,
    const std::string& basePath
)
    : m_vramManager(vramManager),
    m_shaderManager(shaderManager),
    m_materialManager(materialManager),
    m_assetLoader(basePath) {
}

bool AssetManager::ensureLoaded(const AssetHandle handle) {
    if (m_markedForRemoval.count(handle)) {
        m_markedForRemoval.erase(handle);
    }

    // Check if already in cache
    if (m_assetCache.count(handle.filename) == 0) {
        try {
            AssetLib::AssetData data = m_assetLoader.load(handle);
            if (data.header.assetType != handle.type) {
                return false; // Type mismatch
            }
            m_assetCache.emplace(handle.filename, std::move(data));

            // If it's a material, cache it immediately
            if (handle.type == AssetLib::AssetType::Material) {
                if (!cacheMaterial(m_assetCache[handle.filename], handle)) {
                    return false; // Material caching failed
                }
            }
        }
        catch (...) {
            return false; // Loading error
        }
    }

    // Handle material dependencies
    if (handle.type == AssetLib::AssetType::Material) {
        MaterialHandle matHandle = getResource<Material>(handle);
        if (!matHandle) {
            return false;
        }
        const Material* mat = m_materialManager.get(matHandle);
        if (!mat) {
            return false;
        }

        // Ensure shader is loaded
        auto shaderIt = m_materialShaderHandles.find(handle.filename);
        if (shaderIt != m_materialShaderHandles.end()) {
            if (!ensureLoaded(shaderIt->second)) {
                return false;
            }
        }

        // Ensure all textures are loaded
        for (const auto& param : mat->parameters()) {
            if (const Material::TextureParam* tex = std::get_if<Material::TextureParam>(&param.value)) {
                if (!ensureLoaded(tex->handle)) {
                    return false;
                }
            }
        }
    }

    updateLastUsed(handle);
    return true;
}

bool AssetManager::ensureReady(const AssetHandle handle) {
    if (m_markedForRemoval.count(handle)) {
        m_markedForRemoval.erase(handle);
    }

    // Check if already ready
    bool isReady = false;
    switch (handle.type) {
    case AssetLib::AssetType::Mesh:
        isReady = m_vramMeshes.count(handle.filename);
        break;
    case AssetLib::AssetType::Texture:
        isReady = m_vramTextures.count(handle.filename);
        break;
    case AssetLib::AssetType::Material:
        isReady = m_cachedMaterials.count(handle.filename);
        break;
    case AssetLib::AssetType::Shader:
        isReady = m_shaders.count(handle.filename);
        break;
    default:
        return false; // Unknown type
    }

    if (isReady) {
        updateLastUsed(handle);
        return true;
    }

    // Ensure asset is loaded first
    if (!ensureLoaded(handle)) {
        return false;
    }

    // Prepare for use (upload to GPU as needed)
    if (!prepareForUse(handle)) {
        return false; // Preparation failed
    }

    updateLastUsed(handle);

    // For materials, ensure dependencies are ready
    if (handle.type == AssetLib::AssetType::Material) {
        MaterialHandle matHandle = getResource<Material>(handle);
        if (!matHandle) {
            return false;
        }
        Material* mat = m_materialManager.get(matHandle);
        if (!mat) {
            return false;
        }

        // Ensure shader is ready and update material's shader handle
        auto shaderIt = m_materialShaderHandles.find(handle.filename);
        if (shaderIt != m_materialShaderHandles.end()) {
            if (!ensureReady(shaderIt->second)) {
                return false;
            }

            // Update material's shader handle
            const ShaderHandle* shaderHandle = getResource<ShaderHandle>(shaderIt->second);
            if (!shaderHandle) {
                return false;
            }

            mat->shader() = *shaderHandle;
        }

        // Ensure all textures are ready and update VramHandles
        for (auto& param : mat->parameters()) {
            if (auto* texParam = std::get_if<Material::TextureParam>(&param.value)) {
                if (!ensureReady(texParam->handle)) {
                    return false;
                }

                // Update VramHandle in the parameter
                const VramHandle* vramTex = getResource<VramHandle>(texParam->handle);
                if (!vramTex) {
                    return false;
                }
                texParam->vramHandle = *vramTex;
            }
        }

        // Prawdopodonie nie jest to tu potrzebne, 
        // ale zostawiam to tu na razie na wszelki wypadek
        // 
        // Update uniform buffer with all current parameters
        //mat->updateUniformBuffer(m_shaderManager);
    }

    return true;
}


void AssetManager::unloadAsset(const AssetHandle& handle) {
    unloadAssetInternal(handle);

    // Aktualizacja ostatniego użycia
    switch (handle.type) {
    case AssetType::Mesh: m_meshLastUsed.erase(handle.filename); break;
    case AssetType::Texture: m_textureLastUsed.erase(handle.filename); break;
    case AssetType::Shader: m_shaderLastUsed.erase(handle.filename); break;
    case AssetType::Material: m_materialLastUsed.erase(handle.filename); break;
    default: break;
    }
}

void AssetManager::unloadAssetInternal(const AssetHandle& handle) {
    switch (handle.type) {
    case AssetType::Mesh: {
        auto it = m_vramMeshes.find(handle.filename);
        if (it != m_vramMeshes.end()) {
            m_vramManager.freeResource(it->second.vertexBuffer);
            m_vramManager.freeResource(it->second.indexBuffer);
            m_vramMeshes.erase(it);
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

            // Also remove from shader handles map
            m_materialShaderHandles.erase(handle.filename);
        }
        break;
    }
    default:
        throw std::runtime_error("Unknown asset type");
    }
}

void AssetManager::purgeUnusedAssets(float vramThresholdPercentage, uint64_t ageThresholdFrames) {
    // Przetwórz assety oznaczone do usunięcia
    auto markIt = m_markedForRemoval.begin();
    while (markIt != m_markedForRemoval.end()) {
        const AssetHandle& handle = *markIt;

        unloadAssetInternal(handle);
        m_assetCache.erase(handle.filename);

        markIt = m_markedForRemoval.erase(markIt);
    }

    // Zwolnienie zasobów na podstawie wieku
    auto processAgeBasedUnload = [&](auto& lastUsedMap, AssetType type) {
        std::vector<AssetHandle> toUnload;
        for (const auto& [filename, lastUsed] : lastUsedMap) {
            if (m_currentFrame - lastUsed > ageThresholdFrames) {
                toUnload.emplace_back(type, filename);
            }
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

    // Zwolnienie zasobów na podstawie VRAM
    const float currentUsage = m_vramManager.getVramUsagePercentage();
    if (currentUsage > vramThresholdPercentage) {
        struct AssetInfo {
            AssetHandle handle;
            uint64_t size;
            uint64_t lastUsed;
        };

        std::vector<AssetInfo> assets;
        const uint64_t vramBudget = m_vramManager.getVramBudget();
        const uint64_t targetUsage = static_cast<uint64_t>(vramBudget * vramThresholdPercentage);

        // Zbieranie informacji o zasobach
        auto collectAssets = [&](const auto& container, AssetType type) {
            for (const auto& [filename, _] : container) {
                uint64_t size = 0;
                if constexpr (std::is_same_v<std::decay_t<decltype(container)>, decltype(m_vramMeshes)>) {
                    size += m_vramManager.getResourceSize(m_vramMeshes.at(filename).vertexBuffer);
                    size += m_vramManager.getResourceSize(m_vramMeshes.at(filename).indexBuffer);
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

        collectAssets(m_vramMeshes, AssetType::Mesh);
        collectAssets(m_vramTextures, AssetType::Texture);

        // Sortuj od najstarszych
        std::sort(assets.begin(), assets.end(), [](const auto& a, const auto& b) {
            return a.lastUsed < b.lastUsed;
            });

        // Zwolnij aż do osiągnięcia celu
        uint64_t currentVram = m_vramManager.getVramUsed();
        for (const auto& asset : assets) {
            if (currentVram <= targetUsage) break;

            unloadAsset(asset.handle);
            currentVram = m_vramManager.getVramUsed();
        }
    }
}

void AssetManager::releaseAsset(const AssetHandle& handle) {
    // Usuń z załadowanych
    m_assetCache.erase(handle.filename);

    // Oznacz do usunięcia jeśli jest w gotowych
    bool isReady = false;
    switch (handle.type) {
    case AssetType::Mesh: isReady = m_vramMeshes.count(handle.filename); break;
    case AssetType::Texture: isReady = m_vramTextures.count(handle.filename); break;
    case AssetType::Shader: isReady = m_shaders.count(handle.filename); break;
    case AssetType::Material: isReady = m_cachedMaterials.count(handle.filename); break;
    default: break;
    }

    if (isReady) {
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
        break;
    }
}

bool AssetManager::prepareForUse(const AssetHandle& handle) {
    auto it = m_assetCache.find(handle.filename);
    if (it == m_assetCache.end()) {
        if (!ensureLoaded(handle)) {
            return false; // Asset could not be loaded
        }
        it = m_assetCache.find(handle.filename);
        if (it == m_assetCache.end()) {
            return false;
        }
    }

    const AssetLib::AssetData& data = it->second;

    try {
        switch (data.header.assetType) {
        case AssetLib::AssetType::Mesh:
            uploadMesh(data, handle);
            break;
        case AssetLib::AssetType::Texture:
            uploadTexture(data, handle);
            break;
        case AssetLib::AssetType::Shader:
            if (m_shaders.find(handle.filename) == m_shaders.end()) {
                createShader(data, handle);
            }
            break;
        case AssetLib::AssetType::Material:
            // Material should already be cached during ensureLoaded
            if (m_cachedMaterials.find(handle.filename) == m_cachedMaterials.end()) {
                cacheMaterial(data, handle);
            }
            break;
        default:
            return false; // Unknown type
        }
    }
    catch (...) {
        return false; // Error during preparation
    }

    return true;
}

bool AssetManager::uploadMesh(const AssetLib::AssetData& data, const AssetHandle& handle) {
    try {
        auto [meshInfo, vertexData, indexData] = AssetLib::ReadMesh(data);

        // Tworzenie zasobów VRAM
        Graphics::BufferCreateInfo vertexBufferInfo{
            .usage = Graphics::BufferUsageType::Vertex,
            .size = vertexData.size()
        };

        Graphics::BufferCreateInfo indexBufferInfo{
            .usage = Graphics::BufferUsageType::Index,
            .size = indexData.size()
        };

        VramMesh vramMesh;
        vramMesh.vertexBuffer = m_vramManager.createBuffer(
            vertexBufferInfo,
            vertexData.data()
        );

        vramMesh.indexBuffer = m_vramManager.createBuffer(
            indexBufferInfo,
            indexData.data()
        );

        m_vramMeshes.emplace(handle.filename, std::move(vramMesh));
        return true;
    }
    catch (const std::exception& e) {
        // Log the exception message
        std::cerr << "Error uploading mesh: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        return false;
    }
}

bool AssetManager::uploadTexture(const AssetLib::AssetData& data, const AssetHandle& handle) {
    try {
        auto [texInfo, decompressedData] = AssetLib::ReadTexture(data);

        // Mapowanie formatu tekstury
        Graphics::ImageFormat format;
        switch (texInfo.format) {
        case AssetLib::TextureFormat::RGBA8:
            format = Graphics::ImageFormat::R8G8B8A8_UNORM;
            break;
        case AssetLib::TextureFormat::BC7:
            format = Graphics::ImageFormat::BC7_UNORM;
            break;
        default:
            throw std::runtime_error("Unsupported texture format");
        }

        // Konfiguracja obrazu
        Graphics::ImageCreateInfo imageInfo{
            .width = texInfo.width,
            .height = texInfo.height,
            .format = format,
            .usage = Graphics::ImageUsage::TransferDst | Graphics::ImageUsage::Sampled,
            .mipLevels = texInfo.mipLevels,
            .samples = Settings::MsaaSampleCount::Samples1
        };

        // Tworzenie zasobu w VRAM
        VramHandle vramTexture;
        vramTexture = m_vramManager.createImage(
            imageInfo,
            decompressedData.data()
        );

        m_vramTextures.emplace(handle.filename, std::move(vramTexture));
        return true;
    }
    catch (const std::exception& e) {
        // Log the exception message
        std::cerr << "Error uploading texture: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        return false;
    }
}

bool AssetManager::createShader(const AssetLib::AssetData& data, const AssetHandle& handle) {
    try {
        auto [metadata, shaderStages] = AssetLib::ReadShader(data);
        ShaderHandle shaderHandle = m_shaderManager.createShader(metadata, shaderStages);

        // Zapisujemy mapowanie między uchwytem assetu a uchwytem shadera
        m_shaders.emplace(handle.filename, std::move(shaderHandle));
        return true;
    }
    catch (const std::exception& e) {
        // Log the exception message
        std::cerr << "Error creating shader: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        return false;
    }
}

bool AssetManager::cacheMaterial(const AssetLib::AssetData& data, const AssetHandle& handle) {
    if (m_cachedMaterials.find(handle.filename) != m_cachedMaterials.end()) {
        return true; // Already cached
    }

    try {
        // Parse material data from asset
        auto [materialInfo, parameters, parameterData] = AssetLib::ReadMaterial(data);

        // Create shader handle for this material
        std::string shaderName(materialInfo.shaderName.data());
        AssetHandle shaderHandle(AssetType::Shader, shaderName);
        m_materialShaderHandles[handle.filename] = shaderHandle;

        // Ensure the shader is loaded (but not necessarily ready)
        if (!ensureLoaded(shaderHandle)) {
            throw std::runtime_error("Failed to load shader for material: " + handle.filename);
        }

        // Create the shader handle needed for material creation
        ShaderHandle shaderModuleHandle;
        if (ensureReady(shaderHandle)) {
            // If the shader is already ready, get its handle
            shaderModuleHandle = *getResource<ShaderHandle>(shaderHandle);
        }
        else {
            // If the shader is not ready, we'll need to temporarily prepare it
            // This is to avoid circular dependencies when loading multiple materials
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
            return false;
        }

        // For each texture parameter in the material, ensure it's loaded
        Material* material = m_materialManager.get(matHandle);
        if (material) {
            for (auto& param : material->parameters()) {
                if (auto* textureParam = std::get_if<Material::TextureParam>(&param.value)) {
                    // Ensure the texture is loaded
                    ensureLoaded(textureParam->handle);
                }
            }
        }

        m_cachedMaterials[handle.filename] = matHandle;
        updateLastUsed(handle);
        return true;
    }
    catch (const std::exception& e) {
        // Log the exception message
        std::cerr << "Error caching material: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        return false;
    }
}

