#include "MaterialResourceManager.h"
#include <stdexcept>
#include <cassert>
#include "ImageSamplerUtils.h"

MaterialResourceManager::MaterialResourceManager(
    const LogicalDevice& device,
    ShaderModuleManager& shaderModuleManager,
    ImageSamplerManager& samplerManager,
    UniformBufferManager& uniformBufferManager,
    DescriptorAllocator& descriptorAllocator,
    DescriptorLayoutManager& descriptorLayoutManager,
    TextureManager& textureManager
)
    :m_device(device),
    m_shaderModuleManager(shaderModuleManager),
    m_samplerManager(samplerManager),
    m_uniformBufferManager(uniformBufferManager),
    m_descriptorAllocator(descriptorAllocator),
    m_descriptorLayoutManager(descriptorLayoutManager),
    m_textureManager(textureManager){
}

MaterialResourceManager::~MaterialResourceManager() {
    // Resources are managed through MaterialManager, so we don't need to clean up here
}

MaterialResourceManager::MaterialResources MaterialResourceManager::createMaterialResources(
    ShaderHandle shaderHandle,
    const std::vector<Material::Parameter>& parameters
) {
    MaterialResources resources;

    // Create uniform buffer
    resources.uniformBuffer = createMaterialUniformBuffer(shaderHandle, parameters);

    // Create descriptor set
    resources.descriptorSet = createMaterialDescriptorSet(shaderHandle, resources.uniformBuffer, parameters);

    return resources;
}

void MaterialResourceManager::destroyMaterialResources(const MaterialResources& resources) {
    // Release uniform buffer
    m_uniformBufferManager.releaseBuffer(resources.uniformBuffer);

    // Note: Descriptor sets will be freed when the allocator is destroyed
}

UniformBufferHandle MaterialResourceManager::createMaterialUniformBuffer(
    ShaderHandle shaderHandle,
    const std::vector<Material::Parameter>& parameters
) {
    // Get shader metadata to access UBO information
    const auto& metadata = m_shaderModuleManager.getShaderMetadata(shaderHandle);

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
        return UniformBufferHandle(0);
    }

    // Create the buffer
    UniformBufferHandle uboHandle = m_uniformBufferManager.acquireBuffer(*uboInfo);

    // Update buffer with parameter values
    updateUniformBufferFromParameters(uboHandle, *uboInfo, parameters);

    return uboHandle;
}

void MaterialResourceManager::updateUniformBufferFromParameters(
    UniformBufferHandle uboHandle,
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

                // Skip TextureParam
                if constexpr (std::is_same_v<T, Material::TextureParam>) {
                    return;
                }

                // Get size based on the type
                uint32_t size = 0;

                // Use UniformTypeTraits when possible to get size information
                if constexpr (std::is_same_v<T, bool>) {
                    size = ShaderLib::UniformTypeTraits<bool>::size;
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
                    m_uniformBufferManager.updateBuffer(uboHandle, &value, size, variable.offset);
                }
                }, param->value);
        }
    }
}

VkDescriptorSet MaterialResourceManager::createMaterialDescriptorSet(
    ShaderHandle shaderHandle,
    UniformBufferHandle uboHandle,
    const std::vector<Material::Parameter>& parameters
) {
    // Get shader resources to access the descriptor set layout
    const auto& resources = m_shaderModuleManager.getShaderResources(shaderHandle);

    // Find the layout for set 2 (CUSTOM_DESCRIPTOR_SET)
    auto layoutIt = resources.descriptorLayouts.find(ShaderLib::CUSTOM_DESCRIPTOR_SET);
    if (layoutIt == resources.descriptorLayouts.end()) {
        SPDLOG_WARN("No descriptor layout found for set 2 in shader");
        return VK_NULL_HANDLE;
    }

    // Get the descriptor layout
    VkDescriptorSetLayout layout = m_descriptorLayoutManager.get(layoutIt->second);
    if (layout == VK_NULL_HANDLE) {
        SPDLOG_WARN("Invalid descriptor layout for set 2");
        return VK_NULL_HANDLE;
    }

    // Allocate descriptor set
    VkDescriptorSet descriptorSet = m_descriptorAllocator.allocate(layout);
    if (descriptorSet == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Failed to allocate descriptor set for material");
        return VK_NULL_HANDLE;
    }

    // Get UBO buffer for binding
    Buffer* uboBuffer = m_uniformBufferManager.getBuffer(uboHandle);
    if (!uboBuffer) {
        SPDLOG_ERROR("Uniform buffer not found");
        return descriptorSet; // Return incomplete descriptor set
    }

    // Create descriptor writer to update the set
    DescriptorWriter writer;

    // Bind UBO to binding 0
    const auto& bufferInfo = m_uniformBufferManager.getBufferInfo(uboHandle);
    writer.writeBuffer(0, uboBuffer->get(), bufferInfo.size, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

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
        if (!textureParam->textureHandle) {
            SPDLOG_WARN("Texture not loaded for parameter {}", param.name);
            continue;
        }

        // Get image view from texture manager
		VkImageView imageView = m_textureManager.getVkImageView(textureParam->textureHandle);
        if (imageView == VK_NULL_HANDLE) {
            SPDLOG_WARN("Invalid image view for texture parameter {}", param.name);
            continue;
        }

        // Write image descriptor to set
        writer.writeImage(param.binding, imageView, textureParam->sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    // Update the descriptor set
    writer.updateSet(m_device.get(), descriptorSet);

    return descriptorSet;
}

bool MaterialResourceManager::updateTextureBinding(
    const MaterialResources& resources,
    ShaderHandle shaderHandle,
    const std::string& paramName,
    int paramBinding,
    TextureHandle textureHandle,
    VkSampler sampler
) {

    // Create image view for the texture
    VkImageView imageView = m_textureManager.getVkImageView(textureHandle);

    if (imageView == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Failed to create image view for texture");
        return false;
    }

    // Create descriptor writer and update only the specific binding
    DescriptorWriter writer;
    writer.writeImage(
        paramBinding,
        imageView,
        sampler,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    );

    // Update descriptor set
    writer.updateSet(m_device.get(), resources.descriptorSet);

    return true;
}

bool MaterialResourceManager::updateUniformParameter(
    const MaterialResources& resources,
    ShaderHandle shaderHandle,
    const std::string& paramName,
    const Material::ParamValue& value
) {
    // Get shader metadata
    const auto& metadata = m_shaderModuleManager.getShaderMetadata(shaderHandle);

    // Look for the UBO in the shader metadata
    for (const auto& ubo : metadata.customUBOs) {
        if (ubo.set == ShaderLib::CUSTOM_DESCRIPTOR_SET && ubo.binding == 0) {
            // Find the variable in UBO
            for (const auto& var : ubo.variables) {
                if (var.name == paramName) {
                    // Update the variable in UBO using visit pattern
                    std::visit([&](const auto& val) {
                        using T = std::decay_t<decltype(val)>;

                        // Skip TextureParam
                        if constexpr (std::is_same_v<T, Material::TextureParam>) {
                            return;
                        }

                        if constexpr (std::is_same_v<T, bool>) {
                            uint32_t boolVal = val ? 1 : 0;
                            m_uniformBufferManager.updateBuffer(
                                resources.uniformBuffer,
                                &boolVal,
                                sizeof(uint32_t),
                                var.offset
                            );
                            return;
                        }

                        // Otherwise use the value directly
                        m_uniformBufferManager.updateBuffer(
                            resources.uniformBuffer,
                            &val,
                            sizeof(T),
                            var.offset
                        );
                        }, value);
                    return true;
                }
            }
            break;
        }
    }

    return false;
}