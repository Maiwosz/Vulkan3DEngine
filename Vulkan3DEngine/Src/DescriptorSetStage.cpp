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
    m_writer(renderer.vulkanContext().logical(),
        m_samplerManager,
        m_uniformBufferManager,
        m_descriptorAllocator)
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

    // Create object descriptor set
    createObjectDescriptorSet(order, shaderHandle);

    // Assign material descriptor set - assuming MaterialManager returns SmartHandle
    if (order->materialHandle) {
        // If MaterialManager doesn't support SmartHandle yet, this might need adjustment
        auto materialDescriptorSet = m_materialManager.getDescriptorSet(order->materialHandle);
        if (materialDescriptorSet.isValid()) {
            order->materialDescriptorSetHandle = materialDescriptorSet;
        }
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

        // Clear writer for new descriptor set
        m_writer.clear();

        // Write object UBO to descriptor set if available
        if (order->objectUBOHandle.isValid()) {
            m_writer.writeUniformBuffer(0, order->objectUBOHandle);
            SPDLOG_DEBUG("Added object UBO to object descriptor set at binding 0");
        }
        else {
            SPDLOG_WARN("Missing valid object UBO handle in render order");
        }

        // Create descriptor set using writer - this returns SmartHandle automatically
        auto smartObjectDescriptorSet = m_writer.createDescriptorSet(objectSetLayout);

        if (smartObjectDescriptorSet.isValid()) {
            // Store SmartHandle in render order - automatic cleanup when order is destroyed
            order->objectDescriptorSetHandle = smartObjectDescriptorSet;
            SPDLOG_DEBUG("Created object descriptor set with SmartHandle");
        }
        else {
            SPDLOG_ERROR("Failed to create object descriptor set");
        }
    }
    else {
        SPDLOG_DEBUG("No descriptor layout found for object set (set=1)");
    }
}