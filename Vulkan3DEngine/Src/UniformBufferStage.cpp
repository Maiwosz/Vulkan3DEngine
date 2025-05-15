#include "UniformBufferStage.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <glm/gtc/matrix_transform.hpp>
#include "UBOStandardDefinitions.h"

UniformBufferStage::UniformBufferStage(Registry& registry, Renderer& renderer)
    : m_registry(registry)
    , m_shaderManager(renderer.shaderModuleManager())
    , m_uniformBufferManager(renderer.uniformBufferManager())
    , m_materialManager(renderer.materialManager())
{
    SPDLOG_INFO("Initializing UniformBufferStage");
}

void UniformBufferStage::process(std::shared_ptr<RenderOrder> order)
{
    if (!order) {
        SPDLOG_WARN("Attempted to process null render order");
        return;
    }

    SPDLOG_DEBUG("Processing render order of type: {}", renderOrderTypeToString(order->getType()));

    switch (order->getType()) {
    case RenderOrderType::Mesh:
        processMeshOrder(std::static_pointer_cast<MeshRenderOrder>(order));
        break;
    default:
        SPDLOG_WARN("Unknown render order type");
        break;
    }

    // Forward the order to the next stage
    forwardToNextStage(order);
}

void UniformBufferStage::processMeshOrder(std::shared_ptr<MeshRenderOrder> order)
{
    if (!order) {
        SPDLOG_WARN("Null mesh render order provided");
        return;
    }

    // Get material if specified in the order
    Material* material = nullptr;
    if (order->materialHandle) {
        material = m_materialManager.get(order->materialHandle);
        if (!material) {
            SPDLOG_WARN("Invalid material handle in render order: {}", order->materialHandle.id);
        }
    }

    // Get shader handle - either from material or directly from order
    ShaderHandle shaderHandle = material->shader();

    if (!m_shaderManager.isShaderValid(shaderHandle)) {
        SPDLOG_ERROR("Invalid shader handle for mesh render order");
        return;
    }

    // Create and update object UBO
    UniformBufferHandle objectUboHandle = m_uniformBufferManager.acquireBuffer(ShaderLib::OBJECT_UBO);
    if (objectUboHandle) {
        ShaderLib::ObjectUBOData objectUboData;

        auto& transformComponent = m_registry.getComponent<TransformComponent>(order->entity);

        // Fill object UBO data
        objectUboData.model = transformComponent.getModelMatrix();

        // Update buffer with data
        m_uniformBufferManager.updateBuffer(objectUboHandle, &objectUboData, sizeof(objectUboData));

        // Store the handle in the order for later use
        order->objectUBOHandle = objectUboHandle;
    }
    else {
        SPDLOG_WARN("Failed to create object uniform buffer");
    }
}

