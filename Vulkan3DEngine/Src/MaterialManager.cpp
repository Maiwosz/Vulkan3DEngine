#include "MaterialManager.h"
#include <stdexcept>
#include <cassert>
#include "ImageSamplerUtils.h"

MaterialManager::MaterialManager(ShaderModuleManager& shaderModuleManager, ImageSamplerManager& samplerManager)
    : m_shaderModuleManager(shaderModuleManager), m_samplerManager(samplerManager) {
}

MaterialManager::~MaterialManager() {
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

    // Create a custom uniform buffer for this material
    std::string shaderName(materialInfo.shaderName.data());
    UniformBufferHandle uniformBuffer = m_shaderModuleManager.createCustomUniformBuffer(
        shaderHandle,
        "InputData" //nazwa zakodowana na twardo w shaderach
    );

    // Create the material
    MaterialHandle handle(m_nextHandle++);
    auto material = std::make_unique<Material>(name, shaderHandle, materialParams, uniformBuffer);

    // Update the uniform buffer with initial data
    material->updateUniformBuffer(m_shaderModuleManager);

    // Store the material
    m_materials[handle] = std::move(material);

    return handle;
}

void MaterialManager::destroyMaterial(MaterialHandle handle) {
    auto it = m_materials.find(handle);
    if (it != m_materials.end()) {
        // Release the uniform buffer
        if (it->second->uniformBuffer()) {
            m_shaderModuleManager.releaseUniformBuffer(it->second->uniformBuffer());
        }

        // Remove the material
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

bool MaterialManager::setParameter(MaterialHandle handle, const std::string& paramName, const Material::ParamValue& value) {
    auto material = get(handle);
    if (!material) {
        return false;
    }

    bool result = material->setParameter(paramName, value);
    if (result) {
        // Update the uniform buffer if parameter was set successfully
        material->updateUniformBuffer(m_shaderModuleManager);
    }

    return result;
}

void MaterialManager::updateMaterialParameters(MaterialHandle handle) {
    auto material = get(handle);
    if (material) {
        material->updateUniformBuffer(m_shaderModuleManager);
    }
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
    if (assetParam.descriptorType == AssetLib::DescriptorType::CombinedImageSampler) {
        Material::Parameter param;
        convertTextureParameter(param, assetParam, parameterData);
        return param.value;
    }

    // For uniform buffer parameters
    if (assetParam.descriptorType == AssetLib::DescriptorType::UniformBuffer) {
        // Use the uniform type to determine which variant to use
        switch (assetParam.uniformType) {
        case AssetLib::UniformType::Float:
            return Material::FloatParam{ *reinterpret_cast<const float*>(data) };

        case AssetLib::UniformType::Vec2:
        {
            const float* floatData = reinterpret_cast<const float*>(data);
            return Material::Vec2Param{ floatData[0], floatData[1] };
        }

        case AssetLib::UniformType::Vec3:
        {
            const float* floatData = reinterpret_cast<const float*>(data);
            return Material::Vec3Param{ floatData[0], floatData[1], floatData[2] };
        }

        case AssetLib::UniformType::Vec4:
        {
            const float* floatData = reinterpret_cast<const float*>(data);
            return Material::Vec4Param{ floatData[0], floatData[1], floatData[2], floatData[3] };
        }

        case AssetLib::UniformType::Int:
            return Material::IntParam{ *reinterpret_cast<const int32_t*>(data) };

        case AssetLib::UniformType::IVec2:
        {
            const int32_t* intData = reinterpret_cast<const int32_t*>(data);
            return Material::IVec2Param{ intData[0], intData[1] };
        }

        case AssetLib::UniformType::IVec3:
        {
            const int32_t* intData = reinterpret_cast<const int32_t*>(data);
            return Material::IVec3Param{ intData[0], intData[1], intData[2] };
        }

        case AssetLib::UniformType::IVec4:
        {
            const int32_t* intData = reinterpret_cast<const int32_t*>(data);
            return Material::IVec4Param{ intData[0], intData[1], intData[2], intData[3] };
        }

        case AssetLib::UniformType::UInt:
            return Material::UintParam{ *reinterpret_cast<const uint32_t*>(data) };

        case AssetLib::UniformType::UVec2:
        {
            const uint32_t* uintData = reinterpret_cast<const uint32_t*>(data);
            return Material::UVec2Param{ uintData[0], uintData[1] };
        }

        case AssetLib::UniformType::UVec3:
        {
            const uint32_t* uintData = reinterpret_cast<const uint32_t*>(data);
            return Material::UVec3Param{ uintData[0], uintData[1], uintData[2] };
        }

        case AssetLib::UniformType::UVec4:
        {
            const uint32_t* uintData = reinterpret_cast<const uint32_t*>(data);
            return Material::UVec4Param{ uintData[0], uintData[1], uintData[2], uintData[3] };
        }

        case AssetLib::UniformType::Bool:
            return Material::BoolParam{ *reinterpret_cast<const uint32_t*>(data) != 0 };

        case AssetLib::UniformType::Mat2:
        {
            Material::Mat2Param mat;
            const float* floatData = reinterpret_cast<const float*>(data);
            for (int i = 0; i < 2; ++i) {
                for (int j = 0; j < 2; ++j) {
                    mat.data[i][j] = floatData[i * 2 + j];
                }
            }
            return mat;
        }

        case AssetLib::UniformType::Mat3:
        {
            Material::Mat3Param mat;
            const float* floatData = reinterpret_cast<const float*>(data);
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    mat.data[i][j] = floatData[i * 3 + j];
                }
            }
            return mat;
        }

        case AssetLib::UniformType::Mat4:
        {
            Material::Mat4Param mat;
            const float* floatData = reinterpret_cast<const float*>(data);
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    mat.data[i][j] = floatData[i * 4 + j];
                }
            }
            return mat;
        }

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
    AssetLib::DescriptorType descriptorType
) {
    // Get shader metadata from shader module manager
    const auto& metadata = m_shaderModuleManager.getShaderMetadata(shaderHandle);

    // Convert AssetLib descriptor type to ShaderLib descriptor type
    ShaderLib::DescriptorType shaderLibType;
    switch (descriptorType) {
    case AssetLib::DescriptorType::UniformBuffer:
        shaderLibType = ShaderLib::DescriptorType::UniformBuffer;
        break;
    case AssetLib::DescriptorType::CombinedImageSampler:
        shaderLibType = ShaderLib::DescriptorType::CombinedImageSampler;
        break;
        // Add other cases as needed...
    default:
        return 0; // Default binding
    }

    // For UniformBuffer, we know our buffer will use InputData as name
    if (descriptorType == AssetLib::DescriptorType::UniformBuffer) {
        // Look for custom UBO with name "InputData"
        for (const auto& ubo : metadata.customUBOs) {
            if (ubo.name == "InputData") {
                return ubo.binding;
            }
        }
    }

    // For textures, search for matching descriptor name
    if (descriptorType == AssetLib::DescriptorType::CombinedImageSampler) {
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