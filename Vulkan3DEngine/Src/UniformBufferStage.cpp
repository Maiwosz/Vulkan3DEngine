#include "UniformBufferStage.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <glm/gtc/matrix_transform.hpp>
#include <BuiltInBuffers.h>

UniformBufferStage::UniformBufferStage(ProcessingContext& context, Registry& registry, EngineCore& renderer, AssetSystem& assetSystem)
    : ProcessingStage(context)
    , m_registry(registry)
    , m_shaderManager(assetSystem.shaderManager())
    , m_bufferManager(renderer.bufferManager())
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

    // Create object UBO using SmartHandle
    auto smartObjectUbo = m_bufferManager.acquireSmartBuffer(ShaderLib::CreateObjectUBO());
    if (!smartObjectUbo.isValid()) {
        SPDLOG_ERROR("Failed to acquire object uniform buffer");
        return ProcessingResult::Failure;
    }

    // Get transform component
    if (!m_registry.components().hasComponent<TransformComponent>(order->entity)) {
        SPDLOG_ERROR("Entity {} missing TransformComponent", order->entity.id);
        return ProcessingResult::Failure;
    }

    auto& transformComponent = m_registry.components().getComponent<TransformComponent>(order->entity);

    // Create RAII-wrapped writer - automatically unmaps on destruction
    {
        auto writer = m_bufferManager.createMappedWriter(smartObjectUbo.handle());
        if (!writer.isValid()) {
            SPDLOG_ERROR("Failed to create mapped writer for object UBO");
            return ProcessingResult::Failure;
        }

        // Write data using BufferWriter
        if (!writer->write("model", transformComponent.getWorldMatrix())) {
            SPDLOG_ERROR("Failed to write model matrix to object UBO");
            return ProcessingResult::Failure;
        }

        if (!writer->write("color", glm::vec4(1.0f))) {
            SPDLOG_ERROR("Failed to write color to object UBO");
            return ProcessingResult::Failure;
        }

        SPDLOG_DEBUG("Successfully wrote data to object UBO for entity {}", order->entity.id);
    } // writer goes out of scope - buffer is automatically unmapped

    // Store the SmartHandle in the order - automatic cleanup when order is destroyed
    order->objectUBOHandle = smartObjectUbo;

    SPDLOG_DEBUG("Created and updated object UBO for entity {}", order->entity.id);
    return ProcessingResult::Success;
}