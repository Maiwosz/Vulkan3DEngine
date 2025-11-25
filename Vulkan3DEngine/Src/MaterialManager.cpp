#include "MaterialManager.h"
#include <stdexcept>
#include <cassert>
#include "AssetManager.h"
#include <spdlog/spdlog.h>
#include <MaterialSerializer.h>
#include <MaterialTypes.h>

MaterialManager::MaterialManager(
    const LogicalDevice& device,
    ShaderManager& shaderManager,
    ImageSamplerManager& samplerManager,
    BufferManager& uniformBufferManager,
    DescriptorAllocator& descriptorAllocator,
    DescriptorLayoutManager& descriptorLayoutManager,
    TextureManager& textureManager,
    ThreadPool& threadPool
)
    : m_factory(device, shaderManager, uniformBufferManager, samplerManager,
        textureManager, descriptorAllocator, descriptorLayoutManager, threadPool),
    m_shaderManager(shaderManager),
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
        // Read shader name from metadata
        std::string shaderName = AssetLib::GetMaterialShaderName(data);

        // Ensure shader is ready
        AssetHandle shaderHandle(AssetType::Shader, shaderName);
        if (!requestAssetReady(shaderHandle)) {
            SPDLOG_ERROR("MaterialManager: Failed to prepare shader dependency {}", shaderName);
            return false;
        }

        // Get shader handle
        ShaderHandle shader = manager.getHandle<ShaderHandle>(shaderHandle);
        if (!shader.isValid()) {
            SPDLOG_ERROR("MaterialManager: Invalid shader handle for {}", shaderName);
            return false;
        }

        // Read texture dependencies from metadata
        std::vector<std::string> textureNames = AssetLib::GetMaterialTextureDependencies(data);

        // Ensure textures are loaded
        for (const auto& textureName : textureNames) {
            AssetHandle textureHandle(AssetType::Texture, textureName);
            if (!requestAssetLoad(textureHandle)) {
                SPDLOG_WARN("MaterialManager: Failed to load texture dependency {}", textureName);
            }
        }

        // Deserialize full material definition
        AssetLib::MaterialDefinition materialDef = AssetLib::ReadMaterial(data);

        // Create material using factory
        // GPU buffers are created immediately in Material constructor
        MaterialHandle materialHandle = loadMaterialFromAsset(handle, materialDef, shader, manager);
        if (!materialHandle.isValid()) {
            SPDLOG_ERROR("MaterialManager: Failed to create material {}", handle.filename);
            return false;
        }

        auto& materialData = m_materials[materialHandle];
        materialData.isReady = true;

        // Initialize material: sync initial values from asset to GPU
        // GPU buffers are already created, so we can sync immediately
        Material* material = materialData.material.get();
        material->SyncAllToGPU();

        // Note: Descriptor set will be created lazily on first GetDescriptorSet() call
        // This is more efficient as the descriptor set might not be needed immediately

        SPDLOG_DEBUG("MaterialManager: Successfully prepared material {} (GPU buffers created and synced)",
            handle.filename);
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

std::vector<AssetDependency> MaterialManager::getDependencies(
    const AssetHandle& handle, 
    const AssetLib::AssetData& data) const 
{
    std::vector<AssetDependency> dependencies;
    try {
        // Read shader name from metadata
        std::string shaderName = AssetLib::GetMaterialShaderName(data);
        dependencies.push_back({
            AssetHandle(AssetType::Shader, shaderName),
            DependencyType::PrepareTime,
            std::any()
        });
        
        // Read texture dependencies and color spaces from metadata 
        std::vector<std::string> textureNames = AssetLib::GetMaterialTextureDependencies(data);
        auto colorSpaces = AssetLib::GetMaterialTextureColorSpaces(data);
        
        // Add texture dependencies with color space info
        for (const auto& textureName : textureNames) {
            std::unordered_map<std::string, std::any> textureConfig;
            
            // Find color space from metadata
            auto it = colorSpaces.find(textureName);
            if (it != colorSpaces.end()) {
                textureConfig["colorSpace"] = it->second;
            }
            
            dependencies.push_back({
                AssetHandle(AssetType::Texture, textureName),
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
    materialData.estimatedSize = estimateMaterialSize(materialData.material.get());
    materialData.isReady = true;
    materialData.isFromAsset = false;

    m_materials[handle] = std::move(materialData);

    if (!name.empty()) {
        m_filenameToHandle[name] = handle;
    }

    SPDLOG_DEBUG("MaterialManager: Registered runtime material '{}' with handle {}", name, handle.id);
    return handle;
}

SmartAssetHandle<MaterialHandle, Material> MaterialManager::createMaterialFromShader(
    const std::string& shaderName,
    const std::string& materialName
) {
    try {
        // Generate material name from shader name if not provided
        std::string effectiveName = materialName.empty() ? (shaderName + "_material") : materialName;

        // Check if material already exists
        auto existingIt = m_filenameToHandle.find(effectiveName);
        if (existingIt != m_filenameToHandle.end()) {
            MaterialHandle existingHandle = existingIt->second;
            if (isAssetReady(existingHandle)) {
                SPDLOG_DEBUG("MaterialManager: Reusing existing material '{}'", effectiveName);
                auto smartHandle = createSmartHandle(existingHandle);
                if (smartHandle.isValid()) {
                    return smartHandle;
                }
            }
        }

        // 1. Create shader asset handle
        AssetHandle shaderAssetHandle(AssetType::Shader, shaderName);

        // 2. Ensure shader is loaded and ready
        if (!requestAssetReady(shaderAssetHandle)) {
            SPDLOG_ERROR("MaterialManager: Failed to prepare shader '{}'", shaderName);
            return SmartAssetHandle<MaterialHandle, Material>();
        }

        // 3. Get shader handle
        ShaderHandle shaderHandle = m_shaderManager.getHandle<ShaderHandle>(shaderAssetHandle.filename);
        if (!shaderHandle.isValid()) {
            SPDLOG_ERROR("MaterialManager: Invalid shader handle for '{}'", shaderName);
            return SmartAssetHandle<MaterialHandle, Material>();
        }

        // 4. Create material using factory with default buffer instances
        auto material = m_factory.createMaterial(effectiveName, shaderHandle);
        if (!material) {
            SPDLOG_ERROR("MaterialManager: Factory failed to create material '{}'", effectiveName);
            return SmartAssetHandle<MaterialHandle, Material>();
        }

        // 5. Register material and get handle
        MaterialHandle handle = registerMaterial(std::move(material), effectiveName);
        if (!handle.isValid()) {
            SPDLOG_ERROR("MaterialManager: Failed to register material '{}'", effectiveName);
            return SmartAssetHandle<MaterialHandle, Material>();
        }

        // 6. Create and return smart handle
        auto smartHandle = createSmartHandle(handle);
        if (!smartHandle.isValid()) {
            SPDLOG_ERROR("MaterialManager: Failed to create smart handle for '{}'", effectiveName);
            return SmartAssetHandle<MaterialHandle, Material>();
        }

        SPDLOG_DEBUG("MaterialManager: Created material '{}' from shader '{}' with handle {}",
            effectiveName, shaderName, handle.id);
        return smartHandle;

    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("MaterialManager: Exception creating material from shader '{}': {}",
            shaderName, e.what());
        return SmartAssetHandle<MaterialHandle, Material>();
    }
}

SmartAssetHandle<MaterialHandle, Material> MaterialManager::createComputeMaterial(
    const std::string& shaderName
) {
    // Auto-generate material name: "shader_name" -> "shader_name_compute"
    std::string computeMaterialName = shaderName + "_compute";

    SPDLOG_DEBUG("MaterialManager: Creating compute material '{}' from shader '{}'",
        computeMaterialName, shaderName);

    return createMaterialFromShader(shaderName, computeMaterialName);
}

SmartAssetHandle<MaterialHandle, Material> MaterialManager::createMaterialInstance(
    MaterialHandle sourceMaterial,
    const std::string& instanceName
) {
    try {
        // Get source material
        auto* source = getMaterial(sourceMaterial);
        if (!source) {
            SPDLOG_ERROR("MaterialManager: Source material handle {} is invalid",
                sourceMaterial.id);
            return SmartAssetHandle<MaterialHandle, Material>();
        }

        // Check if instance already exists
        auto existingIt = m_filenameToHandle.find(instanceName);
        if (existingIt != m_filenameToHandle.end()) {
            MaterialHandle existingHandle = existingIt->second;
            if (isAssetReady(existingHandle)) {
                SPDLOG_DEBUG("MaterialManager: Reusing existing material instance '{}'",
                    instanceName);
                auto smartHandle = createSmartHandle(existingHandle);
                if (smartHandle.isValid()) {
                    return smartHandle;
                }
            }
        }

        // Clone all buffers from source material
        auto clonedBuffers = m_factory.cloneBuffers(source);

        // Get shader handle from source material
        const auto& shaderHandle = source->GetShader();
        if (!shaderHandle.isValid()) {
            SPDLOG_ERROR("MaterialManager: Source material has invalid shader handle");
            return SmartAssetHandle<MaterialHandle, Material>();
        }

        // Create new material with cloned buffers
        auto instance = m_factory.createMaterial(
            instanceName,
            shaderHandle.handle(),
            clonedBuffers
        );

        if (!instance) {
            SPDLOG_ERROR("MaterialManager: Failed to create material instance '{}'",
                instanceName);
            return SmartAssetHandle<MaterialHandle, Material>();
        }

        // Copy texture bindings from source
        for (const auto& textureName : source->GetTextureNames()) {
            Material::TextureParam textureParam;
            if (source->GetTexture(textureName, textureParam)) {
                instance->SetTexture(textureName, textureParam);
            }
        }

        // Register instance
        MaterialHandle handle = registerMaterial(std::move(instance), instanceName);
        if (!handle.isValid()) {
            SPDLOG_ERROR("MaterialManager: Failed to register material instance '{}'",
                instanceName);
            return SmartAssetHandle<MaterialHandle, Material>();
        }

        // Store source material reference
        auto& materialData = m_materials[handle];
        materialData.sourceMaterialName = source->GetName();

        // Create and return smart handle
        auto smartHandle = createSmartHandle(handle);
        if (!smartHandle.isValid()) {
            SPDLOG_ERROR("MaterialManager: Failed to create smart handle for instance '{}'",
                instanceName);
            return SmartAssetHandle<MaterialHandle, Material>();
        }

        SPDLOG_DEBUG("MaterialManager: Created material instance '{}' from '{}' with handle {}",
            instanceName, source->GetName(), handle.id);
        return smartHandle;

    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("MaterialManager: Exception creating material instance '{}': {}",
            instanceName, e.what());
        return SmartAssetHandle<MaterialHandle, Material>();
    }
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
    materialData.estimatedSize = estimateMaterialSize(materialData.material.get());
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

    for (const auto& textureName : material->GetTextureNames()) {
        Material::TextureParam textureParam;
        if (!material->GetTexture(textureName, textureParam)) {
            continue;
        }

        if (!textureParam.textureHandle.isValid()) {
            try {
                TextureHandle texture = manager.getHandle<TextureHandle>(textureParam.assetHandle);
                if (texture.isValid()) {
                    textureParam.textureHandle = texture;
                    material->SetTexture(textureName, textureParam);
                    anyUpdated = true;
                    SPDLOG_DEBUG("MaterialManager: Updated texture handle for {} in material {}",
                        textureParam.assetHandle.filename, material->GetName());
                }
            }
            catch (const std::exception& e) {
                SPDLOG_DEBUG("MaterialManager: Texture {} still not available",
                    textureParam.assetHandle.filename);
            }
        }
    }

    if (anyUpdated) {
        material->InvalidateDescriptorSet();
    }
}

uint64_t MaterialManager::estimateMaterialSize(const Material* material) const {
    if (!material) {
        return 0;
    }

    uint64_t size = sizeof(Material);

    // Estimate buffer sizes
    for (const auto& bufferName : material->GetBufferNames()) {
        auto buffer = material->GetBuffer(bufferName);
        if (buffer) {
            size += buffer->GetDefinition()->GetTotalSize();
        }
    }

    // Add overhead for texture bindings
    size += material->GetTextureNames().size() * 64; // Rough estimate per texture

    return size;
}
