#include "MaterialManager.h"
#include <stdexcept>
#include <cassert>
#include "AssetManager.h"
#include <spdlog/spdlog.h>
#include <MaterialSerializer.h>

MaterialManager::MaterialManager(
    const LogicalDevice& device,
    ShaderManager& shaderManager,
    ImageSamplerManager& samplerManager,
    BufferManager& uniformBufferManager,
    DescriptorAllocator& descriptorAllocator,
    DescriptorLayoutManager& descriptorLayoutManager,
    TextureManager& textureManager
)
    : m_factory(device, shaderManager, uniformBufferManager, samplerManager,
        textureManager, descriptorAllocator, descriptorLayoutManager),
    m_textureManager(textureManager)
{
}

MaterialManager::~MaterialManager() {
    m_materials.clear();
    m_filenameToHandle.clear();
}

bool MaterialManager::prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) {
    if (handle.type != AssetType::Material) {
        SPDLOG_ERROR("MaterialManager: Invalid asset type for handle {}", handle.filename);
        return false;
    }

    try {
        // Read dependencies from metadata
        AssetLib::MaterialDependencies deps = AssetLib::ReadMaterialDependencies(data);

        // Ensure shader is ready
        AssetHandle shaderHandle(AssetType::Shader, deps.shaderName);
        if (!manager.ensureReady(shaderHandle)) {
            SPDLOG_ERROR("MaterialManager: Failed to prepare shader dependency {}", deps.shaderName);
            return false;
        }

        // Get shader handle
        ShaderHandle shader = manager.getHandle<ShaderHandle>(shaderHandle);
        if (!shader.isValid()) {
            SPDLOG_ERROR("MaterialManager: Invalid shader handle for {}", deps.shaderName);
            return false;
        }

        // Ensure textures are loaded
        for (const auto& textureName : deps.textureNames) {
            AssetHandle textureHandle(AssetType::Texture, textureName);
            if (!manager.ensureLoaded(textureHandle)) {
                SPDLOG_WARN("MaterialManager: Failed to load texture dependency {}", textureName);
            }
        }

        // Deserialize material definition
        AssetLib::MaterialDefinition materialDef = AssetLib::ReadMaterial(data);

        // Create material using factory
        MaterialHandle materialHandle = loadMaterialFromAsset(handle, materialDef, shader, manager);
        if (!materialHandle.isValid()) {
            SPDLOG_ERROR("MaterialManager: Failed to create material {}", handle.filename);
            return false;
        }

        auto& materialData = m_materials[materialHandle];
        materialData.isReady = true;

        SPDLOG_DEBUG("MaterialManager: Successfully prepared material {}", handle.filename);
        return true;

    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("MaterialManager: Exception while preparing material {}: {}",
            handle.filename, e.what());
        return false;
    }
}

void MaterialManager::unloadAsset(const std::string& filename) {
    auto filenameIt = m_filenameToHandle.find(filename);
    if (filenameIt != m_filenameToHandle.end()) {
        MaterialHandle handle = filenameIt->second;

        auto materialIt = m_materials.find(handle);
        if (materialIt != m_materials.end()) {
            m_materials.erase(materialIt);
        }

        m_filenameToHandle.erase(filenameIt);
        SPDLOG_DEBUG("MaterialManager: Unloaded material {}", filename);
    }
}

bool MaterialManager::isAssetReady(const std::string& filename) const {
    auto filenameIt = m_filenameToHandle.find(filename);
    if (filenameIt != m_filenameToHandle.end()) {
        auto materialIt = m_materials.find(filenameIt->second);
        return materialIt != m_materials.end() && materialIt->second.isReady;
    }
    return false;
}

uint64_t MaterialManager::getAssetSize(const std::string& filename) const {
    auto filenameIt = m_filenameToHandle.find(filename);
    if (filenameIt != m_filenameToHandle.end()) {
        auto materialIt = m_materials.find(filenameIt->second);
        if (materialIt != m_materials.end()) {
            return materialIt->second.estimatedSize;
        }
    }
    return 0;
}

std::vector<AssetDependency> MaterialManager::getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const {
    std::vector<AssetDependency> dependencies;

    try {
        // Read dependencies from metadata
        AssetLib::MaterialDependencies deps = AssetLib::ReadMaterialDependencies(data);

        // Add shader dependency
        dependencies.push_back({
            AssetHandle(AssetType::Shader, deps.shaderName),
            DependencyType::PrepareTime,
            std::any()
            });

        // Add texture dependencies
        for (size_t i = 0; i < deps.textureNames.size(); ++i) {
            std::unordered_map<std::string, std::any> textureConfig;
            textureConfig["colorSpace"] = deps.textureColorSpaces[i];

            dependencies.push_back({
                AssetHandle(AssetType::Texture, deps.textureNames[i]),
                DependencyType::UsageTime,
                textureConfig
                });
        }

    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("MaterialManager: Error extracting dependencies for {}: {}",
            handle.filename, e.what());
    }

    return dependencies;
}

std::any MaterialManager::getResourceInternal(const AssetHandle& handle) const {
    auto filenameIt = m_filenameToHandle.find(handle.filename);
    if (filenameIt != m_filenameToHandle.end()) {
        auto materialIt = m_materials.find(filenameIt->second);
        if (materialIt != m_materials.end() && materialIt->second.isReady) {
            return materialIt->second.material.get();
        }
    }
    return std::any();
}

std::any MaterialManager::getHandleInternal(const std::string& filename) const {
    auto filenameIt = m_filenameToHandle.find(filename);
    if (filenameIt != m_filenameToHandle.end()) {
        auto materialIt = m_materials.find(filenameIt->second);
        if (materialIt != m_materials.end() && materialIt->second.isReady) {
            return filenameIt->second;
        }
    }
    return MaterialHandle();
}

Material* MaterialManager::getResource(MaterialHandle handle) const {
    auto it = m_materials.find(handle);
    return (it != m_materials.end() && it->second.isReady) ? it->second.material.get() : nullptr;
}

bool MaterialManager::isAssetReady(MaterialHandle handle) const {
    auto it = m_materials.find(handle);
    return it != m_materials.end() && it->second.isReady;
}

Material* MaterialManager::getMaterial(MaterialHandle handle) {
    auto it = m_materials.find(handle);
    return (it != m_materials.end() && it->second.isReady) ? it->second.material.get() : nullptr;
}

const Material* MaterialManager::getMaterial(MaterialHandle handle) const {
    auto it = m_materials.find(handle);
    return (it != m_materials.end() && it->second.isReady) ? it->second.material.get() : nullptr;
}

MaterialHandle MaterialManager::registerMaterial(std::unique_ptr<Material> material, const std::string& name) {
    if (!material) {
        SPDLOG_ERROR("MaterialManager: Cannot register null material");
        return MaterialHandle();
    }

    MaterialHandle handle(m_nextHandle++);

    MaterialData materialData;
    materialData.material = std::move(material);
    materialData.estimatedSize = sizeof(Material) + 1024;
    materialData.isReady = true;
    materialData.isFromAsset = false;

    m_materials[handle] = std::move(materialData);

    if (!name.empty()) {
        m_filenameToHandle[name] = handle;
    }

    SPDLOG_DEBUG("MaterialManager: Registered runtime material '{}' with handle {}", name, handle.id);
    return handle;
}

MaterialHandle MaterialManager::loadMaterialFromAsset(
    const AssetHandle& assetHandle,
    const AssetLib::MaterialDefinition& materialDef,
    ShaderHandle shaderHandle,
    AssetManager& manager
) {
    // Create material using factory
    auto material = m_factory.createMaterialFromAsset(
        assetHandle.filename,
        shaderHandle,
        materialDef,
        manager
    );

    if (!material) {
        SPDLOG_ERROR("MaterialManager: Factory failed to create material {}", assetHandle.filename);
        return MaterialHandle();
    }

    // Create handle and store material
    MaterialHandle handle(m_nextHandle++);

    MaterialData materialData;
    materialData.material = std::move(material);
    materialData.estimatedSize = sizeof(Material) +
        materialDef.parameters.size() * sizeof(Material::Parameter) + 1024;
    materialData.isReady = false;
    materialData.isFromAsset = true;

    m_materials[handle] = std::move(materialData);
    m_filenameToHandle[assetHandle.filename] = handle;

    return handle;
}

void MaterialManager::updateTextureHandles(MaterialHandle materialHandle, AssetManager& manager) {
    auto it = m_materials.find(materialHandle);
    if (it == m_materials.end() || !it->second.material) {
        return;
    }

    Material* material = it->second.material.get();
    bool anyUpdated = false;

    for (auto& param : material->parameters()) {
        if (auto* textureParam = std::get_if<Material::TextureParam>(&param.value)) {
            if (!textureParam->textureHandle.isValid()) {
                try {
                    TextureHandle texture = manager.getHandle<TextureHandle>(textureParam->handle);
                    if (texture.isValid()) {
                        textureParam->textureHandle = texture;
                        anyUpdated = true;
                        SPDLOG_DEBUG("MaterialManager: Updated texture handle for {} in material {}",
                            textureParam->handle.filename, material->name());
                    }
                }
                catch (const std::exception& e) {
                    SPDLOG_DEBUG("MaterialManager: Texture {} still not available",
                        textureParam->handle.filename);
                }
            }
        }
    }

    if (anyUpdated) {
        material->invalidateDescriptorSet();
    }
}