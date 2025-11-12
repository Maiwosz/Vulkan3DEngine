#include "CameraProcessingStage.h"
#include "CameraComponent.h"
#include "TransformComponent.h"
#include "LightComponent.h"
#include "EngineCore.h"
#include "Buffer.h"
#include <spdlog/spdlog.h>
#include <BuiltInBuffers.h>

CameraProcessingStage::CameraProcessingStage(ProcessingContext& context, Registry& registry, EngineCore& renderer)
    : ProcessingStage(context),
    m_registry(registry),
    m_bufferManager(renderer.bufferManager())
{
    // Create and cache definitions
    m_globalUBODef = ShaderLib::CreateGlobalUBODefinition();

    // Create reusable instance - we'll swap mapped buffer each frame
    m_cachedInstance = m_globalUBODef->CreateInstance();

    // VALIDATE: Ensure C++ struct matches GLSL layout
    try {
        ShaderLib::ValidateGlobalUBOLayout();
        SPDLOG_INFO("GlobalUBO layout validation successful");
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("GlobalUBO layout validation FAILED: {}", e.what());
        throw; // Fatal error - can't continue with mismatched layouts
    }

    SPDLOG_INFO("Initialized CameraProcessingStage with cached instance");
}

ProcessingResult CameraProcessingStage::process(std::shared_ptr<RenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("CameraProcessingStage received null render order");
        return ProcessingResult::Failure;
    }

    if (order->getType() != RenderOrderType::Camera) {
        SPDLOG_DEBUG("CameraProcessingStage skipping non-camera order type: {}",
            renderOrderTypeToString(order->getType()));
        return ProcessingResult::Failure;
    }

    auto cameraOrder = std::static_pointer_cast<CameraRenderOrder>(order);
    return processCameraOrder(cameraOrder);
}

ProcessingResult CameraProcessingStage::processCameraOrder(std::shared_ptr<CameraRenderOrder> cameraOrder) {
    if (!cameraOrder) {
        SPDLOG_ERROR("CameraProcessingStage received null camera order");
        return ProcessingResult::Failure;
    }

    SPDLOG_DEBUG("Processing camera order for entity: {}", cameraOrder->entity.id);

    const auto& allLights = m_context.getLights();
    auto culledLights = cullLights(cameraOrder, allLights);

    SPDLOG_DEBUG("Camera {} culled {} lights from {} total",
        cameraOrder->entity.id, culledLights.size(), allLights.size());

    auto globalUBO = createGlobalUniformBuffer(cameraOrder, culledLights);

    if (globalUBO.isValid()) {
        cameraOrder->globalUBOHandle = globalUBO;
        SPDLOG_DEBUG("Created global UBO for camera entity: {}", cameraOrder->entity.id);
        m_context.addProcessedCamera(cameraOrder);
        return ProcessingResult::Success;
    }
    else {
        SPDLOG_ERROR("Failed to create global UBO for camera entity: {}", cameraOrder->entity.id);
        return ProcessingResult::Failure;
    }
}

SmartHandle<BufferHandle, Buffer> CameraProcessingStage::createGlobalUniformBuffer(
    std::shared_ptr<CameraRenderOrder> cameraOrder,
    const std::vector<std::shared_ptr<LightRenderOrder>>& lights)
{
    Entity cameraEntity = cameraOrder->entity;

    // Validate camera components
    if (!m_registry.components().hasComponent<CameraComponent>(cameraEntity) ||
        !m_registry.components().hasComponent<TransformComponent>(cameraEntity)) {
        SPDLOG_ERROR("Camera entity {} missing required components", cameraEntity.id);
        return SmartHandle<BufferHandle, Buffer>();
    }

    auto& cameraComponent = m_registry.components().getComponent<CameraComponent>(cameraEntity);
    auto& cameraTransform = m_registry.components().getComponent<TransformComponent>(cameraEntity);

    // Acquire buffer
    auto globalUboHandle = m_bufferManager.acquireSmartBuffer(m_globalUBODef);
    if (!globalUboHandle.isValid()) {
        SPDLOG_ERROR("Failed to acquire global uniform buffer for camera {}", cameraEntity.id);
        return SmartHandle<BufferHandle, Buffer>();
    }

    // Reuse cached instance - just update the mapped buffer pointer
    m_cachedInstance->SetMappedBuffer(globalUboHandle.get());

    // Fill C++ struct, then single memcpy
    ShaderLib::GlobalUBOData globalData;
    globalData.SetDefaults(); // Initialize with defaults

    // Set camera data
    globalData.view = cameraTransform.getViewMatrix();

    glm::mat4 projMatrix = cameraComponent.getProjectionMatrix();
    glm::mat4 vulkanCorrection = glm::mat4(1.0f);
    vulkanCorrection[1][1] = -1.0f;
    globalData.proj = vulkanCorrection * projMatrix;

    globalData.cameraPosition = cameraTransform.getPosition();

    // Process lights
    int pointLightCount = 0;
    bool hasDirectionalLight = false;

    for (auto& lightOrder : lights) {
        Entity lightEntity = lightOrder->entity;

        if (!m_registry.components().hasComponent<LightComponent>(lightEntity) ||
            !m_registry.components().hasComponent<TransformComponent>(lightEntity)) {
            SPDLOG_WARN("Light entity {} missing required components", lightEntity.id);
            continue;
        }

        auto& lightComponent = m_registry.components().getComponent<LightComponent>(lightEntity);
        auto& lightTransform = m_registry.components().getComponent<TransformComponent>(lightEntity);

        if (lightComponent.type == LightComponent::Type::Directional && !hasDirectionalLight) {
            // Set directional light
            globalData.directionalLight.direction = glm::normalize(lightComponent.direction);
            globalData.directionalLight.color = lightComponent.getColorWithIntensity();
            hasDirectionalLight = true;
        }
        else if (lightComponent.type == LightComponent::Type::Point && pointLightCount < 64) {
            // Set point light
            auto& pointLight = globalData.pointLights[pointLightCount];
            pointLight.position = lightTransform.getPosition();
            pointLight.radius = lightComponent.radius;
            pointLight.color = lightComponent.getColorWithIntensity();

            SPDLOG_DEBUG("Added point light {} to global UBO, entity: {}, position: ({}, {}, {}), radius: {}, color: ({}, {}, {}, {})",
                pointLightCount, lightEntity.id,
                pointLight.position.x, pointLight.position.y, pointLight.position.z,
                pointLight.radius,
                pointLight.color.r, pointLight.color.g, pointLight.color.b, pointLight.color.a);

            pointLightCount++;
        }
    }

    globalData.activePointLights = pointLightCount;

    // Bezpośrednio kopiuj do GPU - jedno kopiowanie!
    try {
        m_cachedInstance->CopyToGPUDirect(&globalData, 0, sizeof(ShaderLib::GlobalUBOData));
        globalUboHandle->unmap();
        SPDLOG_DEBUG("Directly copied global UBO data to GPU for camera {}", cameraEntity.id);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to copy global UBO to GPU: {}", e.what());
        return SmartHandle<BufferHandle, Buffer>();
    }

    return globalUboHandle;
}

std::vector<std::shared_ptr<LightRenderOrder>> CameraProcessingStage::cullLights(
    std::shared_ptr<CameraRenderOrder> cameraOrder,
    const std::vector<std::shared_ptr<LightRenderOrder>>& allLights) {

    SPDLOG_DEBUG("Light culling for camera {} - pass-through mode (no culling)",
        cameraOrder->entity.id);

    return allLights;
}
