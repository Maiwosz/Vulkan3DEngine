#include "UniformBufferStage.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <glm/gtc/matrix_transform.hpp>
#include "UBOStandardDefinitions.h"

UniformBufferStage::UniformBufferStage(ProcessingContext& context, Registry& registry, EngineCore& renderer, AssetSystem& assetSystem)
    : ProcessingStage(context)
    , m_registry(registry)
    , m_shaderManager(assetSystem.shaderManager())
    , m_uniformBufferManager(renderer.uniformBufferManager())
    , m_materialManager(assetSystem.materialManager())
{
    SPDLOG_INFO("Initializing UniformBufferStage");
}

ProcessingResult UniformBufferStage::process(std::shared_ptr<RenderOrder> order)
{
    if (!order) {
        SPDLOG_WARN("Attempted to process null render order");
        return ProcessingResult::Failure;
    }

    SPDLOG_DEBUG("Processing render order of type: {}", renderOrderTypeToString(order->getType()));

    switch (order->getType()) {
    case RenderOrderType::Mesh:
        return processMeshOrder(std::static_pointer_cast<MeshRenderOrder>(order));
    default:
        SPDLOG_WARN("UniformBufferStage: Unsupported render order type: {}",
            renderOrderTypeToString(order->getType()));
        return ProcessingResult::Failure;
    }
}

ProcessingResult UniformBufferStage::processMeshOrder(std::shared_ptr<MeshRenderOrder> order)
{
    if (!order) {
        SPDLOG_WARN("Null mesh render order provided");
        return ProcessingResult::Failure;
    }

    // Get material if specified in the order
    Material* material = nullptr;
    if (order->materialHandle) {
        material = m_materialManager.getMaterial(order->materialHandle);
        if (!material) {
            SPDLOG_WARN("Invalid material handle in render order: {}", order->materialHandle.id);
            return ProcessingResult::Failure;
        }
    }
    else {
        SPDLOG_WARN("No material handle specified in mesh render order");
        return ProcessingResult::Failure;
    }

    // Get shader handle - either from material or directly from order
    ShaderHandle shaderHandle = material->shader();

    if (!shaderHandle) {
        SPDLOG_ERROR("Invalid shader handle for mesh render order");
        return ProcessingResult::Failure;
    }

    // Create and update object UBO using SmartHandle
    auto smartObjectUbo = m_uniformBufferManager.acquireSmartBuffer(ShaderLib::OBJECT_UBO);
    if (smartObjectUbo.isValid()) {
        ShaderLib::ObjectUBOData objectUboData;

        auto& transformComponent = m_registry.components().getComponent<TransformComponent>(order->entity);

        // Fill object UBO data
        objectUboData.model = transformComponent.getWorldMatrix();

        // Update buffer with data
        m_uniformBufferManager.updateBuffer(smartObjectUbo.handle(), &objectUboData, sizeof(objectUboData));

        // Store the SmartHandle in the order - automatic cleanup when order is destroyed
        order->objectUBOHandle = smartObjectUbo;

        SPDLOG_DEBUG("Created and updated object UBO for entity");
        return ProcessingResult::Success;
    }
    else {
        SPDLOG_WARN("Failed to create object uniform buffer");
        return ProcessingResult::Failure;
    }
}