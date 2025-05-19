#include "DescriptorSetStage.h"
#include "ShaderModuleManager.h"
#include "Material.h"
#include "Buffer.h"

DescriptorSetStage::DescriptorSetStage(
    Renderer& renderer,
    AssetSystem& assetSystem
)
    : m_renderer(renderer),
	m_assetSystem(assetSystem),
    m_shaderManager(assetSystem.shaderManager()),
    m_materialManager(assetSystem.materialManager()),
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
        material = m_materialManager.getMaterial(order->materialHandle);
        if (!material) {
            SPDLOG_WARN("Invalid material handle in render order: {}", order->materialHandle.id);
        }
    }

    // Get shader handle - either from material or directly from order
    if (material) {
        shaderHandle = material->shader();
    }
        

    if (!shaderHandle) {
        SPDLOG_ERROR("Invalid shader handle for mesh render order");
        return;
    }

    // Create global descriptor set
    createGlobalDescriptorSet(order, shaderHandle);

    // Create object descriptor set
    createObjectDescriptorSet(order, shaderHandle);

    // Assign material descriptor set
    if (order->materialHandle) {
        order->materialDescriptorSet = m_materialManager.getMaterialDescriptorSet(order->materialHandle);
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

