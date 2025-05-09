#include "DescriptorSetStage.h"
#include "ShaderModuleManager.h"
#include "Material.h"
#include "Buffer.h"

DescriptorSetStage::DescriptorSetStage(
    Renderer& renderer
)
    : m_renderer(renderer),
    m_shaderManager(renderer.shaderModuleManager()),
    m_materialManager(renderer.materialManager()),
    m_uniformBufferManager(renderer.uniformBufferManager()),
    m_descriptorAllocator(renderer.descriptorAllocator()),
    m_vramManager(renderer.vramManager()),
    m_layoutManager(renderer.descriptorLayoutManager()),
    m_samplerManager(m_renderer.imageSamplerManager()),
    m_writer()
{
    SPDLOG_INFO("Initializing DescriptorSetStage");
}

void DescriptorSetStage::process(std::shared_ptr<RenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("Attempted to process null render order");
        return;
    }

    // Process based on order type
    switch (order->getType()) {
    case RenderOrderType::Mesh: {
        auto meshOrder = std::static_pointer_cast<MeshRenderOrder>(order);
        processMeshOrder(meshOrder);
        break;
    }
    default:
        SPDLOG_WARN("Unknown render order type");
        break;
    }

    // Forward the order to the next stage
    forwardToNextStage(order);
}

void DescriptorSetStage::processMeshOrder(std::shared_ptr<MeshRenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("Null mesh render order provided");
        return;
    }

    SPDLOG_DEBUG("Processing descriptor sets for mesh handle {}", order->meshHandle.id);

    // Get material if specified in the order
    Material* material = nullptr;
    ShaderHandle shaderHandle;
    if (order->materialHandle) {
        material = m_materialManager.get(order->materialHandle);
        if (!material) {
            SPDLOG_WARN("Invalid material handle in render order: {}", order->materialHandle.id);
        }
    }

    // Get shader handle - either from material or directly from order
    if (material) {
        shaderHandle = material->shader();
    }
        

    if (!m_shaderManager.isShaderValid(shaderHandle)) {
        SPDLOG_ERROR("Invalid shader handle for mesh render order");
        return;
    }

    // Create global descriptor set
    createGlobalDescriptorSet(order, shaderHandle);

    // Create object descriptor set
    createObjectDescriptorSet(order, shaderHandle);

    // Create material descriptor set if material is present
    if (material) {
        createMaterialDescriptorSet(order, shaderHandle, material);
    }
}

void DescriptorSetStage::createGlobalDescriptorSet(std::shared_ptr<MeshRenderOrder> order, ShaderHandle shader) {
    // Get shader resources (descriptor layouts, pipeline layout)
    const ShaderResources& shaderResources = m_shaderManager.getShaderResources(shader);

    // Check if we have a layout for global descriptor set (set=1)
    if (shaderResources.descriptorLayouts.count(0) > 0) {
        // Get descriptor set layout for set=1
        VkDescriptorSetLayout globalSetLayout =
            m_layoutManager.get(shaderResources.descriptorLayouts.at(0));

        // Allocate descriptor set using allocator
        VkDescriptorSet globalDescriptorSet = m_descriptorAllocator.allocate(globalSetLayout);

        if (globalDescriptorSet != VK_NULL_HANDLE) {
            // Prepare descriptor writer
            m_writer.clear();

            // Write global UBO to descriptor set if available
            if (order->globalUBOHandle) {
                Buffer* globalBuffer = m_uniformBufferManager.getBuffer(order->globalUBOHandle);
                if (globalBuffer) {
                    m_writer.writeBuffer(0, globalBuffer->get(), globalBuffer->getSize(), 0,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
                    SPDLOG_DEBUG("Added global UBO to global descriptor set at binding 0");
                }
                else {
                    SPDLOG_WARN("Invalid global UBO buffer");
                }
            }
            else {
                SPDLOG_WARN("Missing global UBO handle in render order");
            }

            // Update descriptor set
            m_writer.updateSet(m_renderer.vulkanContext().logical().get(), globalDescriptorSet);

            // Store descriptor set in render order
            order->globalDescriptorSet = globalDescriptorSet;
            SPDLOG_DEBUG("Created global descriptor set");
        }
        else {
            SPDLOG_ERROR("Failed to allocate global descriptor set");
        }
    }
    else {
        SPDLOG_DEBUG("No descriptor layout found for global set (set=1)");
    }
}

void DescriptorSetStage::createObjectDescriptorSet(std::shared_ptr<MeshRenderOrder> order, ShaderHandle shader) {
    // Get shader resources (descriptor layouts, pipeline layout)
    const ShaderResources& shaderResources = m_shaderManager.getShaderResources(shader);

    // Check if we have a layout for object descriptor set (set=1)
    if (shaderResources.descriptorLayouts.count(1) > 0) {
        // Get descriptor set layout for set=1
        VkDescriptorSetLayout objectSetLayout =
            m_layoutManager.get(shaderResources.descriptorLayouts.at(1));

        // Allocate descriptor set using allocator
        VkDescriptorSet objectDescriptorSet = m_descriptorAllocator.allocate(objectSetLayout);

        if (objectDescriptorSet != VK_NULL_HANDLE) {
            // Prepare descriptor writer
            m_writer.clear();

            // Write object UBO to descriptor set if available
            if (order->objectUBOHandle) {
                Buffer* objectBuffer = m_uniformBufferManager.getBuffer(order->objectUBOHandle);
                if (objectBuffer) {
                    m_writer.writeBuffer(0, objectBuffer->get(), objectBuffer->getSize(), 0,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
                    SPDLOG_DEBUG("Added object UBO to object descriptor set at binding 0");
                }
                else {
                    SPDLOG_WARN("Invalid object UBO buffer");
                }
            }
            else {
                SPDLOG_WARN("Missing object UBO handle in render order");
            }

            // Update descriptor set
            m_writer.updateSet(m_renderer.vulkanContext().logical().get(), objectDescriptorSet);

            // Store descriptor set in render order
            order->objectDescriptorSet = objectDescriptorSet;
            SPDLOG_DEBUG("Created object descriptor set");
        }
        else {
            SPDLOG_ERROR("Failed to allocate object descriptor set");
        }
    }
    else {
        SPDLOG_DEBUG("No descriptor layout found for object set (set=1)");
    }
}

void DescriptorSetStage::createMaterialDescriptorSet(std::shared_ptr<MeshRenderOrder> order,
    ShaderHandle shader, Material* material) {
    if (!material) {
        SPDLOG_WARN("Null material provided to createMaterialDescriptorSet");
        return;
    }

    // Get shader resources and metadata
    const ShaderResources& shaderResources = m_shaderManager.getShaderResources(shader);
    const ShaderLib::ShaderMetadata& shaderMetadata = m_shaderManager.getShaderMetadata(shader);

    // Check if we have a layout for material descriptor set (set=2)
    if (shaderResources.descriptorLayouts.count(2) > 0) {
        // Get descriptor set layout for set=2
        VkDescriptorSetLayout materialSetLayout =
            m_layoutManager.get(shaderResources.descriptorLayouts.at(2));

        // Allocate descriptor set using allocator
        VkDescriptorSet materialDescriptorSet = m_descriptorAllocator.allocate(materialSetLayout);

        if (materialDescriptorSet != VK_NULL_HANDLE) {
            // Prepare descriptor writer
            m_writer.clear();

            // Write custom UBO (material data) to descriptor set if available
            if (order->materialUBOHandle) {
                Buffer* customBuffer = m_uniformBufferManager.getBuffer(order->materialUBOHandle);
                if (customBuffer) {
                    m_writer.writeBuffer(0, customBuffer->get(), customBuffer->getSize(), 0,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
                    SPDLOG_DEBUG("Added custom UBO to material descriptor set at binding 0");
                }
                else {
                    SPDLOG_WARN("Invalid custom UBO buffer");
                }
            }
            else {
                SPDLOG_WARN("Missing custom UBO handle in render order");
            }

            // Process texture parameters from material
            const auto& materialParams = material->parameters();
            for (const auto& param : materialParams) {
                if (std::holds_alternative<Material::TextureParam>(param.value)) {
                    const auto& textureParam = std::get<Material::TextureParam>(param.value);

                    // Check if this is a texture with a valid handle
                    if (textureParam.vramHandle.isValid()) {
                        // Find appropriate binding based on shader metadata
                        int binding = -1;

                        // First try direct name match
                        for (const auto& descriptor : shaderMetadata.descriptors) {
                            if (descriptor.name == param.name &&
                                descriptor.set == ShaderLib::CUSTOM_DESCRIPTOR_SET &&
                                descriptor.type == ShaderLib::DescriptorType::CombinedImageSampler) {
                                binding = descriptor.binding;
                                SPDLOG_DEBUG("Found texture binding for '{}' by direct name match: binding={}",
                                    param.name, binding);
                                break;
                            }
                        }

                        // If not found directly, try matching by sampler declaration
                        if (binding < 0) {
                            // Try to find sampler using different naming conventions
                            std::vector<std::string> possibleNames = {
                                param.name,
                                param.name + "Sampler",
                                param.name + "_sampler",
                                "sampler" + param.name.substr(0, 1) + param.name.substr(1)  // samplerDiffuse
                            };

                            for (const auto& name : possibleNames) {
                                for (const auto& descriptor : shaderMetadata.descriptors) {
                                    if (descriptor.name == name &&
                                        descriptor.set == ShaderLib::CUSTOM_DESCRIPTOR_SET &&
                                        descriptor.type == ShaderLib::DescriptorType::CombinedImageSampler) {
                                        binding = descriptor.binding;
                                        SPDLOG_DEBUG("Found texture binding for '{}' by alternative name '{}': binding={}",
                                            param.name, name, binding);
                                        break;
                                    }
                                }
                                if (binding >= 0) break;
                            }
                        }

                        if (binding >= 0) {
                            // Get sampler from textureParam or use default from samplerManager
                            VkSampler sampler = textureParam.sampler;
                            if (sampler == VK_NULL_HANDLE) {
                                // Use default sampler configuration
                                SamplerConfig samplerConfig{}; // Default configuration
                                sampler = m_samplerManager.getSampler(samplerConfig);
                                SPDLOG_DEBUG("Using default sampler for texture '{}'", param.name);
                            }
                            else {
                                SPDLOG_DEBUG("Using provided sampler for texture '{}'", param.name);
                            }

                            Image* textureImage = m_vramManager.getResource<Image>(textureParam.vramHandle);

                            // Get ImageView for texture from image manager
                            VkImageView imageView = textureImage->createView(
                                m_renderer.vulkanContext().logical().get(),
                                VK_IMAGE_VIEW_TYPE_2D
                                // Format will use the image's format by default
                                // AspectMask will be derived from the format automatically
                                // Mip levels will default to VK_REMAINING_MIP_LEVELS
                                // Array layers will default to VK_REMAINING_ARRAY_LAYERS
                            );

                            if (imageView != VK_NULL_HANDLE) {
                                m_writer.writeImage(binding, imageView, sampler,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

                                SPDLOG_DEBUG("Added texture '{}' to material descriptor set at binding {}",
                                    param.name, binding);
                            }
                            else {
                                SPDLOG_ERROR("Failed to get valid ImageView for texture '{}'", param.name);
                            }
                        }
                        else {
                            SPDLOG_WARN("No suitable binding found for texture parameter '{}' in shader metadata",
                                param.name);
                        }
                    }
                    else {
                        SPDLOG_WARN("Texture parameter '{}' has invalid VRAM handle", param.name);
                    }
                }
            }

            // Update descriptor set
            m_writer.updateSet(m_renderer.vulkanContext().logical().get(), materialDescriptorSet);

            // Store descriptor set in render order
            order->materialDescriptorSet = materialDescriptorSet;
            SPDLOG_DEBUG("Created material descriptor set");
        }
        else {
            SPDLOG_ERROR("Failed to allocate material descriptor set");
        }
    }
    else {
        SPDLOG_DEBUG("No descriptor layout found for material set (set=2)");
    }
}