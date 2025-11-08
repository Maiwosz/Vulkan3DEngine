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
    // Create and cache object UBO definition once
    m_objectUBODef = ShaderLib::CreateObjectUBODefinition();

    // Create reusable instance once - we'll just swap the mapped buffer each frame
    m_cachedInstance = m_objectUBODef->CreateInstance();

    // Cache field offsets for direct memory writes (O(1) access, no lookups!)
    const auto* modelField = m_objectUBODef->FindField("model");
    const auto* colorField = m_objectUBODef->FindField("color");

    if (!modelField || !colorField) {
        throw std::runtime_error("Failed to find required fields in object UBO definition");
    }

    m_fieldOffsets.model = modelField->offset;
    m_fieldOffsets.color = colorField->offset;

    SPDLOG_INFO("Initialized UniformBufferStage with cached instance and field offsets");
    SPDLOG_DEBUG("Field offsets - model: {}, color: {}", m_fieldOffsets.model, m_fieldOffsets.color);
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

    // Validate material handle
    if (!order->materialHandle) {
        SPDLOG_WARN("No material handle specified in mesh render order");
        return ProcessingResult::Failure;
    }

    Material* material = m_materialManager.getMaterial(order->materialHandle);
    if (!material) {
        SPDLOG_WARN("Invalid material handle in render order: {}", order->materialHandle.id);
        return ProcessingResult::Failure;
    }

    // Validate transform component
    if (!m_registry.components().hasComponent<TransformComponent>(order->entity)) {
        SPDLOG_ERROR("Entity {} missing TransformComponent", order->entity.id);
        return ProcessingResult::Failure;
    }

    auto& transformComponent = m_registry.components().getComponent<TransformComponent>(order->entity);

    // Acquire buffer
    auto smartObjectUbo = m_bufferManager.acquireSmartBuffer(m_objectUBODef);
    if (!smartObjectUbo.isValid()) {
        SPDLOG_ERROR("Failed to acquire object uniform buffer");
        return ProcessingResult::Failure;
    }

    // Reuse cached instance - just update the mapped buffer pointer
    m_cachedInstance->SetMappedBuffer(smartObjectUbo.get());

    // OPTIMIZATION: Direct memory writes using cached offsets
    // This bypasses all lookups and proxy object creation!
    uint8_t* rawBuffer = m_cachedInstance->GetRawBuffer();

    // Write model matrix directly to cached offset
    glm::mat4 worldMatrix = transformComponent.getWorldMatrix();
    std::memcpy(rawBuffer + m_fieldOffsets.model, &worldMatrix, sizeof(glm::mat4));

    // Write color directly to cached offset
    glm::vec4 color(1.0f, 1.0f, 1.0f, 1.0f);
    std::memcpy(rawBuffer + m_fieldOffsets.color, &color, sizeof(glm::vec4));

    SPDLOG_DEBUG("Set object UBO data for entity {} using direct memory writes", order->entity.id);

    // Synchronize to GPU buffer
    try {
        m_cachedInstance->SyncToBuffer();
        smartObjectUbo->unmap();
        SPDLOG_DEBUG("Successfully synchronized object UBO data to GPU for entity {}", order->entity.id);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to sync object UBO to buffer: {}", e.what());
        return ProcessingResult::Failure;
    }

    // Store the SmartHandle in the order
    order->objectUBOHandle = smartObjectUbo;

    SPDLOG_DEBUG("Created and updated object UBO for entity {}", order->entity.id);
    return ProcessingResult::Success;
}
