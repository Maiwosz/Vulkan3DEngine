#include "MaterialManager.h"
#include <stdexcept>
#include <cassert>
#include "ImageSamplerUtils.h"
#include "AssetManager.h"
#include <spdlog/spdlog.h>

MaterialManager::MaterialManager(
    const LogicalDevice& device,
    ShaderManager& shaderManager,
    ImageSamplerManager& samplerManager,
    UniformBufferManager& uniformBufferManager,
    DescriptorAllocator& descriptorAllocator,
    DescriptorLayoutManager& descriptorLayoutManager,
    TextureManager& textureManager
)
    : m_device(device),
    m_shaderManager(shaderManager),
    m_samplerManager(samplerManager),
    m_textureManager(textureManager) {

    // Create the resource factory
    m_resourceFactory = std::make_unique<MaterialResourceFactory>(
        device,
        shaderManager,
        samplerManager,
        uniformBufferManager,
        descriptorAllocator,
        descriptorLayoutManager,
        textureManager
    );
}

MaterialManager::~MaterialManager() {
    // Smart handles will automatically clean up descriptor sets
    // Clean up all materials
    m_materials.clear();
    m_filenameToHandle.clear();
}

bool MaterialManager::prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) {
    if (handle.type != AssetType::Material) {
        SPDLOG_ERROR("MaterialManager: Invalid asset type for handle {}", handle.filename);
        return false;
    }

    try {
        // Parse material data
        auto [materialInfo, parameters, parameterData] = AssetLib::ReadMaterial(data);

        // Get shader name from material info
        std::string shaderName(materialInfo.shaderName.data());
        AssetHandle shaderHandle(AssetType::Shader, shaderName);

        // Ensure shader is ready (this should be handled by dependency system)
        if (!manager.ensureReady(shaderHandle)) {
            SPDLOG_ERROR("MaterialManager: Failed to prepare shader dependency {}", shaderName);
            return false;
        }

        // Get shader handle from shader module manager using the getHandle method
        ShaderHandle shader = m_shaderManager.getHandle<ShaderHandle>(shaderName);
        if (!shader.isValid()) {
            SPDLOG_ERROR("MaterialManager: Invalid shader handle for {}", shaderName);
            return false;
        }

        // Create material handle
        MaterialHandle materialHandle = createMaterial(handle, data, shader);
        if (!materialHandle.isValid()) {
            SPDLOG_ERROR("MaterialManager: Failed to create material {}", handle.filename);
            return false;
        }

        // Process texture dependencies and ensure they're loaded
        auto& materialData = m_materials[materialHandle];
        for (const auto& param : materialData.material->parameters()) {
            if (std::holds_alternative<Material::TextureParam>(param.value)) {
                auto& textureParam = std::get<Material::TextureParam>(param.value);
                if (!textureParam.handle.filename.empty()) {
                    // Ensure texture is loaded
                    if (!manager.ensureLoaded(textureParam.handle)) {
                        SPDLOG_WARN("MaterialManager: Failed to load texture dependency {}",
                            textureParam.handle.filename);
                        // Continue with preparation even if some textures fail
                    }
                }
            }
        }

        // Update texture handles after textures are loaded
        updateTextureHandles(materialHandle, manager);

        // Collect sampler handles for dirty checking
        collectSamplerHandles(materialHandle);

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
            // Smart handles will automatically clean up when MaterialData is destroyed
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
        // Parse material data to extract dependencies
        auto [materialInfo, parameters, parameterData] = AssetLib::ReadMaterial(data);

        // Add shader dependency - must be prepared before material
        std::string shaderName(materialInfo.shaderName.data());
        dependencies.push_back({
            AssetHandle(AssetType::Shader, shaderName),
            DependencyType::PrepareTime,
            std::any()
            });

        // Add texture dependencies
        for (const auto& param : parameters) {
            if (param.descriptorType == ShaderLib::DescriptorType::CombinedImageSampler) {
                // Extract texture path from parameter data
                if (param.dataSize > 0 && param.dataOffset < parameterData.size()) {
                    const char* texturePath = reinterpret_cast<const char*>(parameterData.data() + param.dataOffset);
                    std::string textureFilename(texturePath);

                    if (!textureFilename.empty()) {
                        // Create configuration for texture with color space info
                        std::unordered_map<std::string, std::any> textureConfig;
                        textureConfig["colorSpace"] = param.samplerDesc.colorSpace;

                        dependencies.push_back({
                            AssetHandle(AssetType::Texture, textureFilename),
                            DependencyType::UsageTime,
                            textureConfig
                            });
                    }
                }
            }
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
    return MaterialHandle(); // Return invalid handle if not found or not ready
}

Material* MaterialManager::getMaterial(MaterialHandle handle) {
    auto it = m_materials.find(handle);
    return (it != m_materials.end() && it->second.isReady) ? it->second.material.get() : nullptr;
}

const Material* MaterialManager::getMaterial(MaterialHandle handle) const {
    auto it = m_materials.find(handle);
    return (it != m_materials.end() && it->second.isReady) ? it->second.material.get() : nullptr;
}

bool MaterialManager::setMaterialParameter(MaterialHandle handle, const std::string& paramName, const Material::ParamValue& value) {
    auto it = m_materials.find(handle);
    if (it == m_materials.end() || !it->second.isReady) {
        SPDLOG_WARN("MaterialManager: Invalid or not ready material handle {} for setParameter", handle.id);
        return false;
    }

    Material* material = it->second.material.get();
    if (!material) {
        SPDLOG_ERROR("MaterialManager: Null material for handle {}", handle.id);
        return false;
    }

    // Set the parameter using Material's method
    bool success = material->setParameter(paramName, value);

    if (success) {
        // Automatically invalidate descriptor set when parameter changes
        invalidateDescriptorSet(handle);

        // If this is a texture parameter, we might need to update sampler handles
        if (std::holds_alternative<Material::TextureParam>(value)) {
            collectSamplerHandles(handle);
        }

        SPDLOG_DEBUG("MaterialManager: Updated parameter '{}' for material {}",
            paramName, material->name());
    }
    else {
        SPDLOG_WARN("MaterialManager: Failed to set parameter '{}' for material {}",
            paramName, material->name());
    }

    return success;
}

bool MaterialManager::getMaterialParameter(MaterialHandle handle, const std::string& paramName, Material::ParamValue& outValue) const {
    auto it = m_materials.find(handle);
    if (it == m_materials.end() || !it->second.isReady) {
        SPDLOG_WARN("MaterialManager: Invalid or not ready material handle {} for getParameter", handle.id);
        return false;
    }

    const Material* material = it->second.material.get();
    if (!material) {
        SPDLOG_ERROR("MaterialManager: Null material for handle {}", handle.id);
        return false;
    }

    return material->getParameter(paramName, outValue);
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> MaterialManager::getDescriptorSet(MaterialHandle handle) {
    auto it = m_materials.find(handle);
    if (it == m_materials.end() || !it->second.isReady) {
        SPDLOG_WARN("MaterialManager: Invalid or not ready material handle {}", handle.id);
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    MaterialData& materialData = it->second;

    // Check if we have a valid descriptor set
    bool needsRecreation = !materialData.descriptorSetValid || !materialData.descriptorSet.isValid();

    // Check if any samplers are dirty
    if (!needsRecreation) {
        for (SamplerHandle samplerHandle : materialData.samplerHandles) {
            if (m_samplerManager.isDirty(samplerHandle)) {
                SPDLOG_DEBUG("MaterialManager: Sampler {} is dirty for material {}, recreating descriptor set",
                    samplerHandle.id, materialData.material->name());
                needsRecreation = true;
                break;
            }
        }
    }

    // If descriptor set is valid and no samplers are dirty, return cached version
    if (!needsRecreation) {
        return materialData.descriptorSet;
    }

    // Create new descriptor set
    try {
        materialData.descriptorSet = m_resourceFactory->createMaterialDescriptorSet(
            materialData.material->shader(),
            materialData.material->parameters()
        );

        if (materialData.descriptorSet.isValid()) {
            materialData.descriptorSetValid = true;

            // Clear dirty flags for all samplers
            for (SamplerHandle samplerHandle : materialData.samplerHandles) {
                m_samplerManager.clearDirty(samplerHandle);
            }

            SPDLOG_DEBUG("MaterialManager: Created descriptor set for material {}",
                materialData.material->name());
        }
        else {
            SPDLOG_ERROR("MaterialManager: Failed to create descriptor set for material {}",
                materialData.material->name());
        }

        return materialData.descriptorSet;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("MaterialManager: Exception while creating descriptor set for material {}: {}",
            materialData.material->name(), e.what());
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }
}

void MaterialManager::invalidateDescriptorSet(MaterialHandle handle) {
    auto it = m_materials.find(handle);
    if (it != m_materials.end()) {
        it->second.descriptorSetValid = false;
        // The old descriptor set will be automatically cleaned up by smart handle
        // when it goes out of scope or when no more references exist
        SPDLOG_DEBUG("MaterialManager: Invalidated descriptor set for material handle {}", handle.id);
    }
}

MaterialHandle MaterialManager::createMaterial(
    const AssetHandle& assetHandle,
    const AssetLib::AssetData& assetData,
    ShaderHandle shaderHandle
) {
    // Parse material data
    auto [materialInfo, parameters, parameterData] = AssetLib::ReadMaterial(assetData);

    // Create material parameters
    std::vector<Material::Parameter> materialParams;
    materialParams.reserve(parameters.size());

    for (const auto& param : parameters) {
        materialParams.push_back(createMaterialParameter(param, parameterData, shaderHandle));
    }

    // Create the material handle
    MaterialHandle handle(m_nextHandle++);
    auto material = std::make_unique<Material>(assetHandle.filename, shaderHandle, materialParams);

    // Calculate estimated size
    uint64_t estimatedSize = sizeof(Material) +
        materialParams.size() * sizeof(Material::Parameter) +
        1024; // Estimate for UBO and descriptor set

    // Store the material data
    MaterialData materialData;
    materialData.material = std::move(material);
    materialData.estimatedSize = estimatedSize;
    materialData.isReady = false;

    // Add to handle-based storage
    m_materials[handle] = std::move(materialData);

    // Add to filename-to-handle mapping
    m_filenameToHandle[assetHandle.filename] = handle;

    return handle;
}

Material::Parameter MaterialManager::createMaterialParameter(
    const AssetLib::MaterialParameter& assetParam,
    const std::vector<uint8_t>& parameterData,
    ShaderHandle shaderHandle
) {
    Material::Parameter param;
    param.name = assetParam.name.data();
    param.descriptorType = assetParam.descriptorType;
    param.uniformType = assetParam.uniformType;

    // Find the appropriate binding for this parameter based on shader metadata
    param.binding = findBindingForParameter(
        shaderHandle,
        param.name,
        assetParam.descriptorType
    );

    param.arrayIndex = assetParam.arraySize > 0 ? 0 : 0;  // Default to first element for arrays

    // Convert the parameter data
    param.value = convertParameter(assetParam, parameterData, assetParam.dataOffset);

    return param;
}

Material::ParamValue MaterialManager::convertParameter(
    const AssetLib::MaterialParameter& assetParam,
    const std::vector<uint8_t>& parameterData,
    uint32_t dataOffset
) {
    // Make sure we don't go out of bounds
    if (dataOffset + assetParam.dataSize > parameterData.size()) {
        throw std::runtime_error("Parameter data out of bounds");
    }

    const void* data = parameterData.data() + dataOffset;

    // For texture parameters
    if (assetParam.descriptorType == ShaderLib::DescriptorType::CombinedImageSampler) {
        // Extract the texture path from parameter data
        if (assetParam.dataSize > 0 && assetParam.dataOffset < parameterData.size()) {
            const char* texturePath = reinterpret_cast<const char*>(parameterData.data() + assetParam.dataOffset);

            // Create a TextureParam with the handle and empty TextureHandle (will be populated later)
            Material::TextureParam textureParam;
            textureParam.handle = AssetHandle(AssetType::Texture, std::string(texturePath));
            textureParam.textureHandle = TextureHandle(); // Will be populated when texture is loaded
            textureParam.colorSpace = assetParam.samplerDesc.colorSpace;

            // Create sampler configuration based on the material's sampler description
            SamplerConfig samplerConfig = ImageSamplerUtils::createSamplerConfig(assetParam.samplerDesc);
            textureParam.samplerHandle = m_samplerManager.acquireSampler(samplerConfig);

            return textureParam;
        }
        else {
            // Invalid texture parameter data - create an empty texture param
            Material::TextureParam emptyTextureParam;
            emptyTextureParam.colorSpace = AssetLib::ColorSpace::SRGB;
            return emptyTextureParam;
        }
    }

    // For uniform buffer parameters
    if (assetParam.descriptorType == ShaderLib::DescriptorType::UniformBuffer) {
        // Use the uniform type to determine which variant to use
        switch (assetParam.uniformType) {
        case ShaderLib::UniformType::Bool:
            return *reinterpret_cast<const uint32_t*>(data) != 0;

        case ShaderLib::UniformType::Float:
            return *reinterpret_cast<const float*>(data);

        case ShaderLib::UniformType::Vec2:
            return *reinterpret_cast<const glm::vec2*>(data);

        case ShaderLib::UniformType::Vec3:
            return *reinterpret_cast<const glm::vec3*>(data);

        case ShaderLib::UniformType::Vec4:
            return *reinterpret_cast<const glm::vec4*>(data);

        case ShaderLib::UniformType::Int:
            return *reinterpret_cast<const int32_t*>(data);

        case ShaderLib::UniformType::IVec2:
            return *reinterpret_cast<const glm::ivec2*>(data);

        case ShaderLib::UniformType::IVec3:
            return *reinterpret_cast<const glm::ivec3*>(data);

        case ShaderLib::UniformType::IVec4:
            return *reinterpret_cast<const glm::ivec4*>(data);

        case ShaderLib::UniformType::UInt:
            return *reinterpret_cast<const uint32_t*>(data);

        case ShaderLib::UniformType::UVec2:
            return *reinterpret_cast<const glm::uvec2*>(data);

        case ShaderLib::UniformType::UVec3:
            return *reinterpret_cast<const glm::uvec3*>(data);

        case ShaderLib::UniformType::UVec4:
            return *reinterpret_cast<const glm::uvec4*>(data);

        case ShaderLib::UniformType::Mat2:
            return *reinterpret_cast<const glm::mat2*>(data);

        case ShaderLib::UniformType::Mat3:
            return *reinterpret_cast<const glm::mat3*>(data);

        case ShaderLib::UniformType::Mat4:
            return *reinterpret_cast<const glm::mat4*>(data);

        default:
            throw std::runtime_error("Unsupported uniform type");
        }
    }

    // Default case
    throw std::runtime_error("Unsupported parameter type");
}

void MaterialManager::updateTextureHandles(MaterialHandle materialHandle, AssetManager& manager) {
    auto& materialData = m_materials[materialHandle];

    for (auto& param : materialData.material->parameters()) {
        if (std::holds_alternative<Material::TextureParam>(param.value)) {
            auto& textureParam = std::get<Material::TextureParam>(param.value);

            if (!textureParam.handle.filename.empty()) {
                // Get the texture handle from TextureManager through AssetManager
                try {
                    TextureHandle texture = manager.getHandle<TextureHandle>(textureParam.handle);
                    if (texture.isValid()) {
                        textureParam.textureHandle = texture;
                        SPDLOG_DEBUG("MaterialManager: Updated texture handle for {} in material {}",
                            textureParam.handle.filename, materialData.material->name());
                    }
                    else {
                        SPDLOG_WARN("MaterialManager: Failed to get texture handle for {} in material {}",
                            textureParam.handle.filename, materialData.material->name());
                    }
                }
                catch (const std::exception& e) {
                    SPDLOG_ERROR("MaterialManager: Exception while updating texture handle for {} in material {}: {}",
                        textureParam.handle.filename, materialData.material->name(), e.what());
                }
            }
        }
    }
}

void MaterialManager::collectSamplerHandles(MaterialHandle materialHandle) {
    auto& materialData = m_materials[materialHandle];
    materialData.samplerHandles.clear();

    // Collect all sampler handles from material parameters
    for (const auto& param : materialData.material->parameters()) {
        if (std::holds_alternative<Material::TextureParam>(param.value)) {
            const auto& textureParam = std::get<Material::TextureParam>(param.value);
            if (textureParam.samplerHandle.isValid()) {
                materialData.samplerHandles.push_back(textureParam.samplerHandle);
            }
        }
    }
}


uint32_t MaterialManager::findBindingForParameter(
    ShaderHandle shaderHandle,
    const std::string& paramName,
    ShaderLib::DescriptorType descriptorType
) {
    // Get shader metadata from shader module manager
    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);

    // For UniformBuffer, we know our buffer will use InputData as name
    if (descriptorType == ShaderLib::DescriptorType::UniformBuffer) {
        // Look for custom UBO with name "InputData"
        for (const auto& ubo : metadata.customUBOs) {
            if (ubo.name == "InputData") {
                return ubo.binding;
            }
        }
    }

    // For textures, search for matching descriptor name
    if (descriptorType == ShaderLib::DescriptorType::CombinedImageSampler) {
        for (const auto& descriptor : metadata.descriptors) {
            if (descriptor.type == descriptorType && descriptor.name == paramName) {
                return descriptor.binding;
            }
        }
    }

    // If not found, log warning and return default binding
    SPDLOG_WARN("Could not find binding for parameter '{}', using default binding 0", paramName);
    return 0;
}