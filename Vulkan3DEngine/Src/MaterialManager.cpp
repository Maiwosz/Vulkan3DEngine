#include "MaterialManager.h"
#include <stdexcept>
#include <cassert>
#include "ImageSamplerUtils.h"

MaterialManager::MaterialManager(
    ShaderModuleManager& shaderModuleManager,
    ImageSamplerManager& samplerManager,
    UniformBufferManager& uniformBufferManager,
    DescriptorAllocator& descriptorAllocator,
    DescriptorLayoutManager& descriptorLayoutManager,
    VramManager& vramManager,
    const LogicalDevice& device
)
    : m_shaderModuleManager(shaderModuleManager),
    m_samplerManager(samplerManager),
    m_vramManager(vramManager),
    m_device(device) {

    // Create the resource manager
    m_resourceManager = std::make_unique<MaterialResourceManager>(
        shaderModuleManager,
        samplerManager,
        uniformBufferManager,
        descriptorAllocator,
        descriptorLayoutManager,
        vramManager,
        device
    );
}

MaterialManager::~MaterialManager() {
    // Release all material resources
    for (auto& [handle, resources] : m_materialResources) {
        m_resourceManager->destroyMaterialResources(resources);
    }

    // Clean up all materials
    m_materials.clear();
}

MaterialHandle MaterialManager::createMaterial(
    const std::string& name,
    const AssetLib::AssetData& assetData,
    ShaderHandle shaderHandle
) {
    // Parse material data
    auto [materialInfo, parameters, parameterData] = AssetLib::ReadMaterial(assetData);

    // Create material parameters
    std::vector<Material::Parameter> materialParams;
    materialParams.reserve(parameters.size());

    for (const auto& param : parameters) {
        // Pass shaderHandle to find proper bindings
        materialParams.push_back(createMaterialParameter(param, parameterData, shaderHandle));
    }

    // Create the material
    MaterialHandle handle(m_nextHandle++);
    auto material = std::make_unique<Material>(name, shaderHandle, materialParams);

    // Store the material
    m_materials[handle] = std::move(material);

    return handle;
}

void MaterialManager::destroyMaterial(MaterialHandle handle) {
    // Release material resources if they exist
    auto resourceIt = m_materialResources.find(handle);
    if (resourceIt != m_materialResources.end()) {
        m_resourceManager->destroyMaterialResources(resourceIt->second);
        m_materialResources.erase(resourceIt);
    }

    // Remove the material
    auto it = m_materials.find(handle);
    if (it != m_materials.end()) {
        m_materials.erase(it);
    }
}

Material* MaterialManager::get(MaterialHandle handle) {
    auto it = m_materials.find(handle);
    return it != m_materials.end() ? it->second.get() : nullptr;
}

const Material* MaterialManager::get(MaterialHandle handle) const {
    auto it = m_materials.find(handle);
    return it != m_materials.end() ? it->second.get() : nullptr;
}

bool MaterialManager::isValid(MaterialHandle handle) const {
    return m_materials.find(handle) != m_materials.end();
}

Material::Parameter MaterialManager::createMaterialParameter(
    const AssetLib::MaterialParameter& assetParam,
    const std::vector<uint8_t>& parameterData,
    ShaderHandle shaderHandle
) {
    Material::Parameter param;
    param.name = assetParam.name.data();
    param.descriptorType = assetParam.descriptorType;

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
        Material::Parameter param;
        convertTextureParameter(param, assetParam, parameterData);
        return param.value;
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

void MaterialManager::convertTextureParameter(
    Material::Parameter& param,
    const AssetLib::MaterialParameter& assetParam,
    const std::vector<uint8_t>& parameterData
) {
    // Extract the texture path from parameter data
    if (assetParam.dataSize > 0 && assetParam.dataOffset < parameterData.size()) {
        // Get the texture path as a null-terminated string
        const char* texturePath = reinterpret_cast<const char*>(parameterData.data() + assetParam.dataOffset);

        // Create a TextureParam with the handle and empty VramHandle (will be populated later)
        Material::TextureParam textureParam;

        // Create proper AssetHandle with the texture path
        // Assuming textures should be of AssetType::Texture
        textureParam.handle = AssetHandle(AssetType::Texture, std::string(texturePath));
        textureParam.vramHandle = VramHandle(); // Will be populated when ensuring the texture is ready

        // Create sampler configuration based on the material's sampler description
        SamplerConfig samplerConfig = ImageSamplerUtils::createSamplerConfig(assetParam.samplerDesc);

        // Get the sampler from the sampler manager
        textureParam.sampler = m_samplerManager.getSampler(samplerConfig);

        // Set the parameter value
        param.value = textureParam;
    }
    else {
        // Invalid texture parameter data - create an empty texture param
        Material::TextureParam emptyTextureParam;
        param.value = emptyTextureParam;
    }
}

uint32_t MaterialManager::findBindingForParameter(
    ShaderHandle shaderHandle,
    const std::string& paramName,
    ShaderLib::DescriptorType descriptorType
) {
    // Get shader metadata from shader module manager
    const auto& metadata = m_shaderModuleManager.getShaderMetadata(shaderHandle);

    // Convert AssetLib descriptor type to ShaderLib descriptor type
    ShaderLib::DescriptorType shaderLibType;
    switch (descriptorType) {
    case ShaderLib::DescriptorType::UniformBuffer:
        shaderLibType = ShaderLib::DescriptorType::UniformBuffer;
        break;
    case ShaderLib::DescriptorType::CombinedImageSampler:
        shaderLibType = ShaderLib::DescriptorType::CombinedImageSampler;
        break;
        // Add other cases as needed...
    default:
        return 0; // Default binding
    }

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
            if (descriptor.type == shaderLibType && descriptor.name == paramName) {
                return descriptor.binding;
            }
        }
    }

    // If not found, log warning and return default binding
    SPDLOG_WARN("Could not find binding for parameter '{}', using default binding 0", paramName);
    return 0;
}

VkDescriptorSet MaterialManager::getMaterialDescriptorSet(MaterialHandle handle) {
    // Get material to ensure it exists
    const Material* material = get(handle);
    if (!material) {
        SPDLOG_ERROR("Cannot get descriptor set - invalid material handle");
        return VK_NULL_HANDLE;
    }

    // Check if resources already exist
    auto resourceIt = m_materialResources.find(handle);
    if (resourceIt != m_materialResources.end()) {
        return resourceIt->second.descriptorSet;
    }

    // If not, create resources for this material
    return getOrCreateMaterialResources(handle)->descriptorSet;
}

MaterialResourceManager::MaterialResources* MaterialManager::getOrCreateMaterialResources(MaterialHandle handle) {
    // Check if resources already exist
    auto resourceIt = m_materialResources.find(handle);
    if (resourceIt != m_materialResources.end()) {
        return &resourceIt->second;
    }

    // Get material
    const Material* material = get(handle);
    if (!material) {
        return nullptr;
    }

    // Create material resources using the resource manager
    MaterialResourceManager::MaterialResources resources = m_resourceManager->createMaterialResources(
        material->shader(),
        material->parameters()
    );

    // Store resources and return them
    auto [it, inserted] = m_materialResources.emplace(handle, resources);
    return inserted ? &it->second : nullptr;
}