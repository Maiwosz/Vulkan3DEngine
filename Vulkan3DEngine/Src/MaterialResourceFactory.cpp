#include "MaterialResourceFactory.h"
#include <stdexcept>
#include <cassert>
#include "ImageSamplerUtils.h"
#include <BufferDefinitions.h>
#include <spdlog/spdlog.h>

MaterialResourceFactory::MaterialResourceFactory(
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
    m_uniformBufferManager(uniformBufferManager),
    m_descriptorAllocator(descriptorAllocator),
    m_descriptorLayoutManager(descriptorLayoutManager),
    m_textureManager(textureManager) {
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> MaterialResourceFactory::createMaterialDescriptorSet(
    ShaderHandle shaderHandle,
    const std::vector<Material::Parameter>& parameters
) {
    // Create uniform buffer
    auto uboHandle = createMaterialUniformBuffer(shaderHandle, parameters);

    // Create descriptor set
    return createDescriptorSetInternal(shaderHandle, uboHandle, parameters);
}

SmartHandle<UniformBufferHandle, Buffer> MaterialResourceFactory::createMaterialUniformBuffer(
    ShaderHandle shaderHandle,
    const std::vector<Material::Parameter>& parameters
) {
    // Get shader metadata to access UBO information
    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);

    // Look for UBO in the custom UBOs (should be in set CUSTOM_DESCRIPTOR_SET = 2)
    const ShaderLib::UniformBufferObject* uboInfo = nullptr;
    for (const auto& ubo : metadata.customUBOs) {
        if (ubo.set == ShaderLib::CUSTOM_DESCRIPTOR_SET && ubo.binding == 0) {
            uboInfo = &ubo;
            break;
        }
    }

    // If no UBO found, return invalid handle
    if (!uboInfo) {
        SPDLOG_WARN("No uniform buffer found in set 2 binding 0 for shader used by material");
        return SmartHandle<UniformBufferHandle, Buffer>();
    }

    // Create the buffer using smart handle
    auto uboHandle = m_uniformBufferManager.acquireSmartBuffer(*uboInfo);

    // Update buffer with parameter values
    updateUniformBufferFromParameters(uboHandle, *uboInfo, parameters);

    return uboHandle;
}

void MaterialResourceFactory::updateUniformBufferFromParameters(
    const SmartHandle<UniformBufferHandle, Buffer>& uboHandle,
    const ShaderLib::UniformBufferObject& uboInfo,
    const std::vector<Material::Parameter>& parameters
) {
    // Create a map of parameter name to parameter for easy lookup
    std::unordered_map<std::string, const Material::Parameter*> paramMap;
    for (const auto& param : parameters) {
        if (param.descriptorType == ShaderLib::DescriptorType::UniformBuffer) {
            paramMap[param.name] = &param;
        }
    }

    // For each variable in the UBO, try to match a parameter
    for (const auto& variable : uboInfo.variables) {
        auto it = paramMap.find(variable.name);
        if (it != paramMap.end()) {
            const Material::Parameter* param = it->second;

            // Based on the variable type, update the buffer
            std::visit([&](const auto& value) {
                using T = std::decay_t<decltype(value)>;

                // Skip TextureParam - not stored in UBO
                if constexpr (std::is_same_v<T, Material::TextureParam>) {
                    return;
                }

                // Get size based on the type using UniformTypeTraits
                uint32_t size = 0;

                if constexpr (std::is_same_v<T, bool>) {
                    // Convert bool to uint32_t for shader compatibility
                    uint32_t boolVal = value ? 1 : 0;
                    size = ShaderLib::UniformTypeTraits<bool>::size;
                    m_uniformBufferManager.updateBuffer(uboHandle.handle(), &boolVal, size, variable.offset);
                    return;
                }
                else if constexpr (std::is_same_v<T, float>) {
                    size = ShaderLib::UniformTypeTraits<float>::size;
                }
                else if constexpr (std::is_same_v<T, glm::vec2>) {
                    size = ShaderLib::UniformTypeTraits<glm::vec2>::size;
                }
                else if constexpr (std::is_same_v<T, glm::vec3>) {
                    size = ShaderLib::UniformTypeTraits<glm::vec3>::size;
                }
                else if constexpr (std::is_same_v<T, glm::vec4>) {
                    size = ShaderLib::UniformTypeTraits<glm::vec4>::size;
                }
                else if constexpr (std::is_same_v<T, int32_t>) {
                    size = ShaderLib::UniformTypeTraits<int>::size;
                }
                else if constexpr (std::is_same_v<T, glm::ivec2>) {
                    size = ShaderLib::UniformTypeTraits<glm::ivec2>::size;
                }
                else if constexpr (std::is_same_v<T, glm::ivec3>) {
                    size = ShaderLib::UniformTypeTraits<glm::ivec3>::size;
                }
                else if constexpr (std::is_same_v<T, glm::ivec4>) {
                    size = ShaderLib::UniformTypeTraits<glm::ivec4>::size;
                }
                else if constexpr (std::is_same_v<T, uint32_t>) {
                    size = ShaderLib::UniformTypeTraits<unsigned int>::size;
                }
                else if constexpr (std::is_same_v<T, glm::uvec2>) {
                    size = ShaderLib::UniformTypeTraits<glm::uvec2>::size;
                }
                else if constexpr (std::is_same_v<T, glm::uvec3>) {
                    size = ShaderLib::UniformTypeTraits<glm::uvec3>::size;
                }
                else if constexpr (std::is_same_v<T, glm::uvec4>) {
                    size = ShaderLib::UniformTypeTraits<glm::uvec4>::size;
                }
                else if constexpr (std::is_same_v<T, glm::mat2>) {
                    size = ShaderLib::UniformTypeTraits<glm::mat2>::size;
                }
                else if constexpr (std::is_same_v<T, glm::mat3>) {
                    size = ShaderLib::UniformTypeTraits<glm::mat3>::size;
                }
                else if constexpr (std::is_same_v<T, glm::mat4>) {
                    size = ShaderLib::UniformTypeTraits<glm::mat4>::size;
                }

                // Update the buffer directly using the value
                if (size > 0) {
                    m_uniformBufferManager.updateBuffer(uboHandle.handle(), &value, size, variable.offset);
                }
                }, param->value);
        }
    }
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> MaterialResourceFactory::createDescriptorSetInternal(
    ShaderHandle shaderHandle,
    const SmartHandle<UniformBufferHandle, Buffer>& uboHandle,
    const std::vector<Material::Parameter>& parameters
) {
    // Get shader resources to access the descriptor set layout
    const auto& resources = m_shaderManager.getShaderResources(shaderHandle);

    // Find the layout for set 2 (CUSTOM_DESCRIPTOR_SET)
    auto layoutIt = resources.descriptorLayouts.find(ShaderLib::CUSTOM_DESCRIPTOR_SET);
    if (layoutIt == resources.descriptorLayouts.end()) {
        SPDLOG_WARN("No descriptor layout found for set 2 in shader");
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    // Get the descriptor layout
    VkDescriptorSetLayout layout = m_descriptorLayoutManager.get(layoutIt->second);
    if (layout == VK_NULL_HANDLE) {
        SPDLOG_WARN("Invalid descriptor layout for set 2");
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    // Create descriptor writer
    DescriptorWriter writer(m_device, m_samplerManager, m_uniformBufferManager, m_descriptorAllocator);

    // Bind UBO to binding 0 if valid
    if (uboHandle.isValid()) {
        writer.writeUniformBuffer(0, uboHandle);
    }

    // Bind textures to their respective bindings
    for (const auto& param : parameters) {
        // Skip non-texture parameters
        if (param.descriptorType != ShaderLib::DescriptorType::CombinedImageSampler) {
            continue;
        }

        // Get texture parameter
        const Material::TextureParam* textureParam = std::get_if<Material::TextureParam>(&param.value);
        if (!textureParam) {
            continue;
        }

        // Check if texture is loaded
        if (!textureParam->textureHandle.isValid()) {
            SPDLOG_WARN("Texture not loaded for parameter {}", param.name);
            continue;
        }

        // Get image view from texture manager
        VkImageView imageView = m_textureManager.getImageView(textureParam->textureHandle);
        if (imageView == VK_NULL_HANDLE) {
            SPDLOG_WARN("Invalid image view for texture parameter {}", param.name);
            continue;
        }

        // Write combined image sampler to descriptor set
        writer.writeCombinedImageSampler(param.binding, imageView, textureParam->samplerHandle);
    }

    // Create descriptor set with all bindings
    return writer.createDescriptorSet(layout);
}