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

    // VALIDATE: Ensure C++ struct matches GLSL layout
    try {
        ShaderLib::ValidateObjectUBOLayout();
        SPDLOG_INFO("ObjectUBO layout validation successful");
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("ObjectUBO layout validation FAILED: {}", e.what());
        throw; // Fatal error - can't continue with mismatched layouts
    }

    SPDLOG_INFO("Initialized UniformBufferStage with cached instance");
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

    // Use C++ struct for single memcpy
    ShaderLib::ObjectUBOData objectData;
    objectData.model = transformComponent.getWorldMatrix();
    objectData.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    // Bezpośrednio kopiuj do GPU - jedno kopiowanie zamiast dwóch!
    try {
        m_cachedInstance->CopyToGPUDirect(&objectData, 0, sizeof(ShaderLib::ObjectUBOData));
        SPDLOG_DEBUG("Directly copied object UBO data to GPU for entity {}", order->entity.id);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to copy object UBO to GPU: {}", e.what());
        return ProcessingResult::Failure;
    }

    // Store the SmartHandle in the order
    order->objectUBOHandle = smartObjectUbo;

    SPDLOG_DEBUG("Created and updated object UBO for entity {}", order->entity.id);
    return ProcessingResult::Success;
}
