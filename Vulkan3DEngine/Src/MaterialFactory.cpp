#include "MaterialFactory.h"
#include "ImageSamplerUtils.h"
#include "AssetManager.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

MaterialFactory::MaterialFactory(
    const LogicalDevice& device,
    ShaderManager& shaderManager,
    BufferManager& bufferManager,
    ImageSamplerManager& samplerManager,
    TextureManager& textureManager,
    DescriptorAllocator& descriptorAllocator,
    DescriptorLayoutManager& descriptorLayoutManager
)
    : m_device(device),
    m_shaderManager(shaderManager),
    m_bufferManager(bufferManager),
    m_samplerManager(samplerManager),
    m_textureManager(textureManager),
    m_descriptorAllocator(descriptorAllocator),
    m_descriptorLayoutManager(descriptorLayoutManager)
{
}

std::unique_ptr<Material> MaterialFactory::createMaterial(
    const std::string& name,
    ShaderHandle shaderHandle
) {
    auto parameters = createDefaultParameters(shaderHandle);
    return createMaterial(name, shaderHandle, parameters);
}

std::unique_ptr<Material> MaterialFactory::createMaterial(
    const std::string& name,
    ShaderHandle shaderHandle,
    const std::vector<Material::Parameter>& parameters
) {
    if (!shaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Invalid shader handle for material: " + name);
    }

    auto smartShaderHandle = createSmartShaderHandle(shaderHandle);
    if (!smartShaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Failed to create smart shader handle for material: " + name);
    }

    return std::make_unique<Material>(
        name,
        smartShaderHandle,
        parameters,
        m_device,
        m_bufferManager,
        m_samplerManager,
        m_textureManager,
        m_descriptorAllocator,
        m_descriptorLayoutManager
    );
}

std::unique_ptr<Material> MaterialFactory::createMaterialFromAsset(
    const std::string& name,
    ShaderHandle shaderHandle,
    const AssetLib::MaterialDefinition& materialDef,
    AssetManager& assetManager
) {
    if (!materialDef.Validate()) {
        throw std::runtime_error("MaterialFactory: Invalid material definition for: " + name);
    }

    std::vector<Material::Parameter> parameters;
    parameters.reserve(materialDef.parameters.size());

    for (const auto& assetParam : materialDef.parameters) {
        parameters.push_back(createParameterFromAsset(assetParam, shaderHandle, assetManager));
    }

    return createMaterial(name, shaderHandle, parameters);
}

Material::Parameter MaterialFactory::createParameterFromAsset(
    const AssetLib::ParameterValue& assetParam,
    ShaderHandle shaderHandle,
    AssetManager& assetManager
) {
    Material::Parameter param;
    param.name = assetParam.name;
    param.descriptorType = assetParam.descriptorType;
    param.binding = findBindingForParameter(shaderHandle, param.name, assetParam.descriptorType);
    param.value = convertParameterValue(assetParam, assetManager);

    return param;
}

std::vector<Material::Parameter> MaterialFactory::createDefaultParameters(ShaderHandle shaderHandle) {
    std::vector<Material::Parameter> parameters;

    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        SPDLOG_WARN("MaterialFactory: No custom descriptor set found in shader, creating material with no parameters");
        return parameters;
    }

    // Create default parameters for ALL buffers (uniform AND storage)
    for (const auto& [bufferName, buffer] : customSet->buffers) {
        // Determine descriptor type based on buffer type
        ShaderLib::DescriptorType descriptorType = buffer.IsUniformBuffer()
            ? ShaderLib::DescriptorType::UniformBuffer
            : ShaderLib::DescriptorType::StorageBuffer;

        // Find binding for this buffer
        uint32_t binding = 0;
        for (const auto& slot : customSet->slots) {
            if (slot.name == bufferName) {
                binding = slot.binding;
                break;
            }
        }

        // Create parameters for each variable in the buffer
        for (const auto& variable : buffer.variables) {
            Material::Parameter param;
            param.name = variable.name;
            param.descriptorType = descriptorType;
            param.binding = binding;

            // Handle composite types (structs/arrays)
            if (variable.IsComposite()) {
                // Create default instance from definition
                auto instance = variable.composite->CreateInstance();

                if (variable.composite->IsStruct()) {
                    auto structInstance = std::static_pointer_cast<ShaderLib::ShaderStructInstance>(instance);
                    param.value = ShaderLib::BufferValue(structInstance);
                }
                else if (variable.composite->IsArray()) {
                    auto arrayInstance = std::static_pointer_cast<ShaderLib::ShaderArrayInstance>(instance);
                    param.value = ShaderLib::BufferValue(arrayInstance);
                }
                else {
                    SPDLOG_WARN("MaterialFactory: Unknown composite type for variable '{}', skipping", variable.name);
                    continue;
                }
            }
            // Handle base types
            else {
                switch (variable.baseType) {
                    // Scalar types
                case ShaderLib::BaseType::Bool:
                    param.value = ShaderLib::BufferValue(false);
                    break;
                case ShaderLib::BaseType::Float:
                    param.value = ShaderLib::BufferValue(0.0f);
                    break;
                case ShaderLib::BaseType::Int:
                    param.value = ShaderLib::BufferValue(0);
                    break;
                case ShaderLib::BaseType::UInt:
                    param.value = ShaderLib::BufferValue(0u);
                    break;
                case ShaderLib::BaseType::Double:
                    param.value = ShaderLib::BufferValue(0.0);
                    break;

                    // Float vectors
                case ShaderLib::BaseType::Vec2:
                    param.value = ShaderLib::BufferValue(glm::vec2(0.0f));
                    break;
                case ShaderLib::BaseType::Vec3:
                    param.value = ShaderLib::BufferValue(glm::vec3(0.0f));
                    break;
                case ShaderLib::BaseType::Vec4:
                    param.value = ShaderLib::BufferValue(glm::vec4(0.0f));
                    break;

                    // Integer vectors
                case ShaderLib::BaseType::IVec2:
                    param.value = ShaderLib::BufferValue(glm::ivec2(0));
                    break;
                case ShaderLib::BaseType::IVec3:
                    param.value = ShaderLib::BufferValue(glm::ivec3(0));
                    break;
                case ShaderLib::BaseType::IVec4:
                    param.value = ShaderLib::BufferValue(glm::ivec4(0));
                    break;

                    // Unsigned integer vectors
                case ShaderLib::BaseType::UVec2:
                    param.value = ShaderLib::BufferValue(glm::uvec2(0u));
                    break;
                case ShaderLib::BaseType::UVec3:
                    param.value = ShaderLib::BufferValue(glm::uvec3(0u));
                    break;
                case ShaderLib::BaseType::UVec4:
                    param.value = ShaderLib::BufferValue(glm::uvec4(0u));
                    break;

                    // Double vectors
                case ShaderLib::BaseType::DVec2:
                    param.value = ShaderLib::BufferValue(glm::dvec2(0.0));
                    break;
                case ShaderLib::BaseType::DVec3:
                    param.value = ShaderLib::BufferValue(glm::dvec3(0.0));
                    break;
                case ShaderLib::BaseType::DVec4:
                    param.value = ShaderLib::BufferValue(glm::dvec4(0.0));
                    break;

                    // Matrices (identity matrices as default)
                case ShaderLib::BaseType::Mat2:
                    param.value = ShaderLib::BufferValue(glm::mat2(1.0f));
                    break;
                case ShaderLib::BaseType::Mat3:
                    param.value = ShaderLib::BufferValue(glm::mat3(1.0f));
                    break;
                case ShaderLib::BaseType::Mat4:
                    param.value = ShaderLib::BufferValue(glm::mat4(1.0f));
                    break;

                    // Atomic types
                case ShaderLib::BaseType::AtomicUInt:
                    // Note: Atomic counters typically start at 0
                    param.value = ShaderLib::BufferValue(0u);
                    break;

                    // These should never appear here (handled above or invalid)
                case ShaderLib::BaseType::Struct:
                case ShaderLib::BaseType::Array:
                case ShaderLib::BaseType::Unknown:
                default:
                    SPDLOG_WARN("MaterialFactory: Unsupported or invalid type '{}' for variable '{}', skipping",
                        ShaderLib::BaseTypeToString(variable.baseType), variable.name);
                    continue;
                }
            }

            parameters.push_back(param);
        }
    }

    // Create default parameters for textures (empty handles)
    for (const auto& slot : customSet->slots) {
        if (!ShaderLib::IsTexture(slot.type)) {
            continue;
        }

        Material::Parameter param;
        param.name = slot.name;
        param.descriptorType = slot.type;
        param.binding = slot.binding;

        Material::TextureParam textureParam;
        textureParam.colorSpace = AssetLib::ColorSpace::SRGB;
        param.value = textureParam;

        parameters.push_back(param);
    }

    SPDLOG_DEBUG("MaterialFactory: Created {} default parameters from shader", parameters.size());
    return parameters;
}

uint32_t MaterialFactory::findBindingForParameter(
    ShaderHandle shaderHandle,
    const std::string& paramName,
    ShaderLib::DescriptorType descriptorType
) {
    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        SPDLOG_WARN("MaterialFactory: No custom descriptor set found for parameter '{}', using binding 0", paramName);
        return 0;
    }

    // For ANY buffer type (uniform OR storage), search through buffers
    if (ShaderLib::IsBuffer(descriptorType)) {
        for (const auto& [bufferName, buffer] : customSet->buffers) {
            // Check if buffer type matches requested descriptor type
            bool typeMatches = (descriptorType == ShaderLib::DescriptorType::UniformBuffer && buffer.IsUniformBuffer()) ||
                (descriptorType == ShaderLib::DescriptorType::StorageBuffer && buffer.IsStorageBuffer());

            if (!typeMatches) {
                continue;
            }

            // Search for variable in this buffer
            for (const auto& variable : buffer.variables) {
                if (variable.name == paramName) {
                    // Found variable - now find the slot binding
                    for (const auto& slot : customSet->slots) {
                        if (slot.name == bufferName && slot.type == descriptorType) {
                            return slot.binding;
                        }
                    }
                }
            }
        }
    }

    // For textures, search for matching descriptor slot
    if (ShaderLib::IsTexture(descriptorType)) {
        for (const auto& slot : customSet->slots) {
            if (slot.type == descriptorType && slot.name == paramName) {
                return slot.binding;
            }
        }
    }

    SPDLOG_WARN("MaterialFactory: Could not find binding for parameter '{}' (type: {}), using binding 0",
        paramName, ShaderLib::DescriptorTypeToString(descriptorType));
    return 0;
}

Material::ParamValue MaterialFactory::convertParameterValue(
    const AssetLib::ParameterValue& assetParam,
    AssetManager& assetManager
) {
    // For texture parameters
    if (assetParam.IsTexture() && assetParam.IsTexturePath()) {
        const std::string& texturePath = assetParam.GetTexturePath();

        Material::TextureParam textureParam;
        textureParam.handle = AssetHandle(AssetType::Texture, texturePath);
        textureParam.colorSpace = assetParam.samplerDesc.colorSpace;

        // Try to get texture handle if already loaded
        try {
            TextureHandle texture = assetManager.getHandle<TextureHandle>(textureParam.handle);
            if (texture.isValid()) {
                textureParam.textureHandle = texture;
                SPDLOG_DEBUG("MaterialFactory: Resolved texture handle for {}", texturePath);
            }
        }
        catch (const std::exception& e) {
            SPDLOG_DEBUG("MaterialFactory: Texture {} not yet loaded, will resolve later", texturePath);
        }

        // Create sampler
        SamplerConfig samplerConfig = ImageSamplerUtils::createSamplerConfig(assetParam.samplerDesc);
        textureParam.samplerHandle = m_samplerManager.acquireSampler(samplerConfig);

        return textureParam;
    }

    // For buffer parameters
    if (assetParam.IsBuffer()) {
        return assetParam.GetBaseValue();
    }

    throw std::runtime_error("MaterialFactory: Unsupported parameter type for: " + assetParam.name);
}

SmartAssetHandle<ShaderHandle, ShaderAsset> MaterialFactory::createSmartShaderHandle(ShaderHandle shaderHandle) {
    auto* shaderManager = dynamic_cast<ISmartAssetHandler<ShaderHandle, ShaderAsset>*>(&m_shaderManager);
    if (!shaderManager) {
        throw std::runtime_error("MaterialFactory: ShaderManager does not support smart handles");
    }

    return shaderManager->createSmartHandle(shaderHandle);
}