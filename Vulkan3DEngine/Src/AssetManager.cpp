#include "AssetManager.h"
#include <json.hpp>
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

    if (m_assetCache.count(handle.filename) == 0) {
        try {
            AssetLib::AssetData data = m_assetLoader.load(handle);
            if (data.header.assetType != handle.type) {
                return false; // Błąd typu
            }
            m_assetCache.emplace(handle.filename, std::move(data));
        }
        catch (...) {
            return false; // Błąd ładowania
        }
    }

    if (handle.type == AssetLib::AssetType::Material) {
        MaterialHandle matHandle = getResource<Material>(handle);
        if (!matHandle) {
            return false;
        }
        const Material* mat = m_materialManager.get(matHandle);
        if (!mat) {
            return false;
        }

        auto shaderIt = m_materialShaderHandles.find(handle.filename);
        if (shaderIt != m_materialShaderHandles.end()) {
            if (!ensureLoaded(shaderIt->second)) {
                return false;
            }
        }

        for (const auto& param : mat->parameters()) {
            if (const Material::TextureParam* tex = std::get_if<Material::TextureParam>(&param.value)) {
                if (!ensureLoaded(tex->handle)) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool AssetManager::ensureReady(const AssetHandle handle) {
    if (m_markedForRemoval.count(handle)) {
        m_markedForRemoval.erase(handle);
    }

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
        isReady = m_combinedShaders.count(handle.filename);
        break;
    default:
        return false; // Nieznany typ
    }

    if (isReady) {
        updateLastUsed(handle);
        return true;
    }

    if (!prepareForUse(handle)) {
        return false; // Przygotowanie nieudane
    }
    updateLastUsed(handle);

    if (handle.type == AssetLib::AssetType::Material) {
        MaterialHandle matHandle = getResource<Material>(handle);
        if (!matHandle) {
            return false;
        }
        const Material* mat = m_materialManager.get(matHandle);
        if (!mat) {
            return false;
        }

        auto shaderIt = m_materialShaderHandles.find(handle.filename);
        if (shaderIt != m_materialShaderHandles.end()) {
            if (!ensureReady(shaderIt->second)) {
                return false;
            }
        }

        for (const auto& param : mat->parameters()) {
            if (const Material::TextureParam* tex = std::get_if<Material::TextureParam>(&param.value)) {
                if (!ensureReady(tex->handle)) {
                    return false;
                }
            }
        }
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
            m_vramManager.freeResource(it->second.image);
            m_vramTextures.erase(it);
        }
        break;
    }
    if (handle.type == AssetType::Shader) {
        if (auto it = m_combinedShaders.find(handle.filename); it != m_combinedShaders.end()) {
            for (auto& [stage, shaderHandle] : it->second.stages) {
                m_shaderManager.destroy(shaderHandle);
            }
            m_combinedShaders.erase(it);
        }
    }
    case AssetType::Material: {
        auto it = m_cachedMaterials.find(handle.filename);
        if (it != m_cachedMaterials.end()) {
            m_materialManager.freeResource(it->second);
            m_cachedMaterials.erase(it);
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
                    size += m_vramManager.getResourceSize(m_vramTextures.at(filename).image);
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
    case AssetType::Shader: isReady = m_combinedShaders.count(handle.filename); break;
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
        return false; // Asset niezaładowany
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
            if (m_combinedShaders.find(handle.filename) == m_combinedShaders.end()) {
                createShader(data, handle);
            }
            break;
        case AssetLib::AssetType::Material:
            cacheMaterial(data, handle);
            break;
        default:
            return false; // Nieznany typ
        }
    }
    catch (...) {
        return false; // Błąd podczas przygotowania
    }

    return true;
}

void AssetManager::uploadMesh(const AssetLib::AssetData& data, const AssetHandle& handle) {
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
}

void AssetManager::uploadTexture(const AssetLib::AssetData& data, const AssetHandle& handle) {
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
    VramTexture vramTexture;
    vramTexture.image = m_vramManager.createImage(
        imageInfo,
        decompressedData.data()
    );

    m_vramTextures.emplace(handle.filename, std::move(vramTexture));
}

void AssetManager::createShader(const AssetLib::AssetData& data, const AssetHandle& handle) {
    auto [stagesInfo, source, flags] = AssetLib::ReadShader(data);
    CombinedShader combinedShader;

    for (const auto& stageInfo : stagesInfo) {
        // Przekaż spirvVersion do parsera metadanych
        ShaderReflection reflection = parseShaderMetadata(stageInfo.reflectionData, stageInfo.spirvVersion);

        std::vector<uint32_t> spirv(stageInfo.spirvCode.size() / sizeof(uint32_t));
        memcpy(spirv.data(), stageInfo.spirvCode.data(), stageInfo.spirvCode.size());

        ShaderModuleHandle shaderHandle = m_shaderManager.createFromSPIRV(spirv, reflection);
        combinedShader.stages[stageInfo.stage] = shaderHandle;
    }

    m_combinedShaders[handle.filename] = std::move(combinedShader);
}

void AssetManager::cacheMaterial(const AssetLib::AssetData& data, const AssetHandle& handle) {
    auto [matInfo, params, paramData] = AssetLib::ReadMaterial(data);

    // Ekstrakcja nazwy shadera
    std::string shaderName(
        matInfo.shaderName.data(),
        strnlen(matInfo.shaderName.data(), matInfo.shaderName.size())
    );
    AssetHandle shaderHandle(AssetType::Shader, shaderName);
    m_materialShaderHandles[handle.filename] = shaderHandle;
    ensureReady(shaderHandle);

    // Konwersja parametrów do formatu Material
    std::vector<Material::Parameter> parameters;
    parameters.reserve(params.size());

    for (const auto& srcParam : params) {
        Material::Parameter dstParam;
        dstParam.name = std::string(
            srcParam.name.data(),
            strnlen(srcParam.name.data(), srcParam.name.size())
        );
        dstParam.arraySize = srcParam.arraySize;

        using Type = AssetLib::MaterialParameter::Type;
        switch (srcParam.type) {
        case Type::Float: {
            float value;
            std::memcpy(&value, paramData.data() + srcParam.dataOffset, sizeof(float));
            dstParam.value = value;
            break;
        }
        case Type::Float2: {
            glm::vec2 vec;
            std::memcpy(&vec, paramData.data() + srcParam.dataOffset, 2 * sizeof(float));
            dstParam.value = vec;
            break;
        }
        case Type::Float3: {
            glm::vec3 vec;
            std::memcpy(&vec, paramData.data() + srcParam.dataOffset, 3 * sizeof(float));
            dstParam.value = vec;
            break;
        }
        case Type::Float4: {
            glm::vec4 vec;
            std::memcpy(&vec, paramData.data() + srcParam.dataOffset, 4 * sizeof(float));
            dstParam.value = vec;
            break;
        }
        case Type::Int: {
            int32_t value;
            std::memcpy(&value, paramData.data() + srcParam.dataOffset, sizeof(int32_t));
            dstParam.value = value;
            break;
        }
        case Type::UInt: {
            uint32_t value;
            std::memcpy(&value, paramData.data() + srcParam.dataOffset, sizeof(uint32_t));
            dstParam.value = value;
            break;
        }
        case Type::Bool: {
            dstParam.value = static_cast<bool>(paramData[srcParam.dataOffset]);
            break;
        }
        case Type::Texture: {
            const char* pathStart = reinterpret_cast<const char*>(
                paramData.data() + srcParam.dataOffset
                );
            std::string texturePath(pathStart);

            Material::TextureParam texParam;
            texParam.handle = AssetHandle(AssetType::Texture, texturePath);
            texParam.sampler = srcParam.samplerDesc;
            dstParam.value = texParam;
            break;
        }
        default:
            throw std::runtime_error("Unsupported material parameter type: " +
                std::to_string(static_cast<int>(srcParam.type)));
        }

        parameters.emplace_back(std::move(dstParam));
    }

    const CombinedShader* shader = getResource<CombinedShader>(shaderHandle);

    // Tworzenie i cache'owanie materiału
    auto material = std::make_unique<Material>(*shader, std::move(parameters));
    MaterialHandle materialHandle = m_materialManager.cacheMaterial(handle.filename, std::move(material));

    // Aktualizacja stanu w AssetManager
    m_cachedMaterials[handle.filename] = materialHandle;
    m_materialLastUsed[handle.filename] = m_currentFrame;
}

ShaderReflection AssetManager::parseShaderMetadata(const std::vector<uint8_t>& metadata, uint32_t spirvVersion) {
    ShaderReflection reflection;
    const uint8_t* data = metadata.data();
    size_t offset = 0;

    // Parsowanie nagłówka
    AssetLib::ShaderReflection reflHeader;
    memcpy(&reflHeader, data + offset, sizeof(reflHeader));
    offset += sizeof(reflHeader);

    // Parsowanie descriptor bindings
    for (uint32_t i = 0; i < reflHeader.descriptorBindingsCount; ++i) {
        AssetLib::DescriptorBinding binding;
        memcpy(&binding, data + offset, sizeof(binding));
        offset += sizeof(binding);

        DescriptorBindingInfo info;
        info.binding = binding.binding;
        info.type = static_cast<DescriptorBindingInfo::Type>(binding.type);
        info.stageFlags = static_cast<uint8_t>(binding.stageFlags);
        info.size = binding.size;
        info.name = std::string(binding.name.data(), binding.name.size());
        reflection.descriptorBindings.push_back(info);
    }

    // Parsowanie push constants
    for (uint32_t i = 0; i < reflHeader.pushConstantRangeCount; ++i) {
        AssetLib::PushConstantRange pc;
        memcpy(&pc, data + offset, sizeof(pc));
        offset += sizeof(pc);

        PushConstantRangeInfo info;
        info.stageFlags = static_cast<uint8_t>(pc.stageFlags);
        info.offset = pc.offset;
        info.size = pc.size;
        reflection.pushConstants.push_back(info);
    }

    reflection.entryPoint = "main"; // GLSL zawsze używa "main" jako domyślnego entry point
    reflection.spirvVersion = spirvVersion; // Wersja z ShaderStageInfo

    return reflection;
}