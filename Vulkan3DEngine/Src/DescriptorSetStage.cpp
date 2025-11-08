#include "DescriptorSetStage.h"
#include "ShaderModuleManager.h"
#include "Material.h"
#include "Buffer.h"
#include <spdlog/spdlog.h>

DescriptorSetStage::DescriptorSetStage(
    ProcessingContext& context,
    EngineCore& renderer,
    AssetSystem& assetSystem
)
    : ProcessingStage(context),
    m_renderer(renderer),
    m_assetSystem(assetSystem),
    m_shaderManager(assetSystem.shaderManager()),
    m_materialManager(assetSystem.materialManager()),
    m_bufferManager(renderer.bufferManager()),
    m_descriptorAllocator(renderer.descriptorAllocator()),
    m_vramManager(renderer.vramManager()),
    m_layoutManager(renderer.descriptorLayoutManager()),
    m_samplerManager(m_renderer.imageSamplerManager()),
    m_writer(renderer.vulkanContext().logical(),
        m_samplerManager,
        m_bufferManager,
        m_descriptorAllocator)
{
    SPDLOG_INFO("Initialized DescriptorSetStage");
}

ProcessingResult DescriptorSetStage::process(std::shared_ptr<RenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("DescriptorSetStage received null render order");
        return ProcessingResult::Failure;
    }

    // Process based on order type
    switch (order->getType()) {
    case RenderOrderType::Mesh: {
        auto meshOrder = std::static_pointer_cast<MeshRenderOrder>(order);
        return processMeshOrder(meshOrder);
    }
    case RenderOrderType::Camera: {
        auto cameraOrder = std::static_pointer_cast<CameraRenderOrder>(order);
        return processCameraOrder(cameraOrder);
    }
    default:
        SPDLOG_WARN("DescriptorSetStage received unknown render order type: {}",
            renderOrderTypeToString(order->getType()));
        return ProcessingResult::Failure; // Unknown order types should be rejected
    }
}

ProcessingResult DescriptorSetStage::processMeshOrder(std::shared_ptr<MeshRenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("Null mesh render order provided");
        return ProcessingResult::Failure;
    }

    SPDLOG_DEBUG("Processing descriptor sets for mesh entity: {}", order->entity.id);

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
        shaderHandle = material->GetShader().handle();
    }

    if (!shaderHandle) {
        SPDLOG_ERROR("Invalid shader handle for mesh render order entity: {}", order->entity.id);
        return ProcessingResult::Failure;
    }

    // Create object descriptor set (set=1 typically)
    if (!createObjectDescriptorSet(order, shaderHandle)) {
        SPDLOG_ERROR("Failed to create object descriptor set for mesh entity: {}", order->entity.id);
        return ProcessingResult::Failure;
    }

    // Assign material descriptor set if available
    if (order->materialHandle) {
        auto material = m_materialManager.getMaterial(order->materialHandle);
        auto materialDescriptorSet = material->GetDescriptorSet();
        if (materialDescriptorSet.isValid()) {
            order->drawCall->setCustomDescriptorSet(materialDescriptorSet);
            SPDLOG_DEBUG("Assigned material descriptor set to mesh entity: {}", order->entity.id);
        }
        else {
            SPDLOG_WARN("Failed to get material descriptor set for mesh entity: {}", order->entity.id);
            return ProcessingResult::Failure;
        }
    }

    return ProcessingResult::Success;
}

ProcessingResult DescriptorSetStage::processCameraOrder(std::shared_ptr<CameraRenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("Null camera render order provided");
        return ProcessingResult::Failure;
    }

    SPDLOG_DEBUG("Processing descriptor sets for camera entity: {}", order->entity.id);

    // Camera should have global UBO from previous stage
    if (!order->globalUBOHandle.isValid()) {
        SPDLOG_ERROR("Camera order missing global UBO handle, entity: {}", order->entity.id);
        return ProcessingResult::Failure;
    }

    // Create global descriptor set for this camera
    if (!createGlobalDescriptorSet(order)) {
        SPDLOG_ERROR("Failed to create global descriptor set for camera entity: {}", order->entity.id);
        return ProcessingResult::Failure;
    }

    return ProcessingResult::Success;
}

bool DescriptorSetStage::createObjectDescriptorSet(std::shared_ptr<MeshRenderOrder> order, ShaderHandle shader) {
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
            m_writer.writeBuffer(0, order->objectUBOHandle);
            SPDLOG_DEBUG("Added object UBO to object descriptor set at binding 0 for entity: {}",
                order->entity.id);
        }
        else {
            SPDLOG_WARN("Missing valid object UBO handle for mesh entity: {}", order->entity.id);
            return false;
        }

        // Create descriptor set using writer - this returns SmartHandle automatically
        auto smartObjectDescriptorSet = m_writer.createDescriptorSet(objectSetLayout);

        if (smartObjectDescriptorSet.isValid()) {
            // Store SmartHandle in render order - automatic cleanup when order is destroyed
            order->drawCall->setObjectDescriptorSet(smartObjectDescriptorSet);
            SPDLOG_DEBUG("Created object descriptor set with SmartHandle for entity: {}",
                order->entity.id);
            return true;
        }
        else {
            SPDLOG_ERROR("Failed to create object descriptor set for entity: {}", order->entity.id);
            return false;
        }
    }
    else {
        SPDLOG_DEBUG("No descriptor layout found for object set (set=1) for entity: {}",
            order->entity.id);
        return true; // Not an error if no descriptor set is needed
    }
}

bool DescriptorSetStage::createGlobalDescriptorSet(std::shared_ptr<CameraRenderOrder> order) {
    if (!order->globalUBOHandle.isValid()) {
        SPDLOG_ERROR("Cannot create global descriptor set without valid global UBO for camera: {}",
            order->entity.id);
        return false;
    }

    try {
        // Get global descriptor set layout from built-in layouts
        VkDescriptorSetLayout globalLayout =
            m_layoutManager.getBuiltInVkLayout(DescriptorLayoutManager::BuiltInLayout::Global);

        if (globalLayout == VK_NULL_HANDLE) {
            SPDLOG_ERROR("Global descriptor set layout not found for camera: {}", order->entity.id);
            return false;
        }

        // Clear writer for new descriptor set
        m_writer.clear();

        // Write global UBO to descriptor set
        m_writer.writeBuffer(0, order->globalUBOHandle);

        // Create descriptor set using writer - this returns SmartHandle automatically
        auto globalDescriptorSet = m_writer.createDescriptorSet(globalLayout);

        if (globalDescriptorSet.isValid()) {
            // Store SmartHandle in camera order
            order->globalDescriptorSetHandle = globalDescriptorSet;
            SPDLOG_DEBUG("Created global descriptor set for camera entity: {}", order->entity.id);
            return true;
        }
        else {
            SPDLOG_ERROR("Failed to create global descriptor set for camera: {}", order->entity.id);
            return false;
        }
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception while creating global descriptor set for camera {}: {}",
            order->entity.id, e.what());
        return false;
    }
}
