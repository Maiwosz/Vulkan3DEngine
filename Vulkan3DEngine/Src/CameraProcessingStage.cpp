#include "CameraProcessingStage.h"
#include "CameraComponent.h"
#include "TransformComponent.h"
#include "LightComponent.h"
#include "EngineCore.h"
#include "Buffer.h"
#include <StandardBufferDefinitions.h>
#include <spdlog/spdlog.h>

CameraProcessingStage::CameraProcessingStage(ProcessingContext& context, Registry& registry, EngineCore& renderer)
    : ProcessingStage(context),
    m_registry(registry),
    m_uniformBufferManager(renderer.uniformBufferManager())
{
    SPDLOG_INFO("Initialized CameraProcessingStage");
}

ProcessingResult CameraProcessingStage::process(std::shared_ptr<RenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("CameraProcessingStage received null render order");
        return ProcessingResult::Failure;
    }

    if (order->getType() != RenderOrderType::Camera) {
        SPDLOG_DEBUG("CameraProcessingStage skipping non-camera order type: {}",
            renderOrderTypeToString(order->getType()));
        return ProcessingResult::Failure; // Not an error - just not our type to process
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

    // Get all lights from processing context
    const auto& allLights = m_context.getLights();

    // Perform light culling for this camera
    auto culledLights = cullLights(cameraOrder, allLights);

    SPDLOG_DEBUG("Camera {} culled {} lights from {} total",
        cameraOrder->entity.id, culledLights.size(), allLights.size());

    // Create global uniform buffer for this camera with culled lights
    auto globalUBO = createGlobalUniformBuffer(cameraOrder, culledLights);

    if (globalUBO.isValid()) {
        // Store the global UBO in camera order
        cameraOrder->globalUBOHandle = globalUBO;
        SPDLOG_DEBUG("Created global UBO for camera entity: {}", cameraOrder->entity.id);

        // Add processed camera to context
        m_context.addProcessedCamera(cameraOrder);
        return ProcessingResult::Success;
    }
    else {
        SPDLOG_ERROR("Failed to create global UBO for camera entity: {}", cameraOrder->entity.id);
        return ProcessingResult::Failure;
    }
}

SmartHandle<UniformBufferHandle, Buffer> CameraProcessingStage::createGlobalUniformBuffer(
    std::shared_ptr<CameraRenderOrder> cameraOrder,
    const std::vector<std::shared_ptr<LightRenderOrder>>& lights) {

    Entity cameraEntity = cameraOrder->entity;

    // Get camera components from registry
    if (!m_registry.components().hasComponent<CameraComponent>(cameraEntity) ||
        !m_registry.components().hasComponent<TransformComponent>(cameraEntity)) {
        SPDLOG_ERROR("Camera entity {} missing required components", cameraEntity.id);
        return SmartHandle<UniformBufferHandle, Buffer>();
    }

    auto& cameraComponent = m_registry.components().getComponent<CameraComponent>(cameraEntity);
    auto& cameraTransform = m_registry.components().getComponent<TransformComponent>(cameraEntity);

    // Initialize global UBO data
    ShaderLib::GlobalUBOData globalUboData;
    globalUboData.SetDefaults();

    // Configure view and projection matrices
    globalUboData.view = cameraTransform.getViewMatrix();

    glm::mat4 projMatrix = cameraComponent.getProjectionMatrix();

    // Apply Vulkan coordinate system correction
    glm::mat4 vulkanCorrection = glm::mat4(1.0f);
    vulkanCorrection[1][1] = -1.0f; // Flip Y for Vulkan coordinate system

    globalUboData.proj = vulkanCorrection * projMatrix;
    globalUboData.cameraPosition = cameraTransform.getPosition();

    // Process lights and populate UBO data
    int pointLightCount = 0;
    int spotLightCount = 0;
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
            // Process directional light (only first one)
            globalUboData.directionalLight.direction = glm::normalize(lightComponent.direction);
            globalUboData.directionalLight.color = lightComponent.color;
            hasDirectionalLight = true;

            SPDLOG_DEBUG("Added directional light to global UBO, entity: {}", lightEntity.id);
        }
        else if (lightComponent.type == LightComponent::Type::Point && pointLightCount < 64) {
            // Process point light
            globalUboData.pointLights[pointLightCount].position = lightTransform.getPosition();
            globalUboData.pointLights[pointLightCount].radius = lightComponent.radius;
            globalUboData.pointLights[pointLightCount].color = lightComponent.color;

            SPDLOG_DEBUG("Added point light to global UBO, entity: {}, position: ({}, {}, {}), radius: {}",
                lightEntity.id,
                lightTransform.getPosition().x,
                lightTransform.getPosition().y,
                lightTransform.getPosition().z,
                lightComponent.radius);

            pointLightCount++;
        }
    }

    // Set active light counts
    globalUboData.activePointLights = pointLightCount;
    globalUboData.activeSpotLights = spotLightCount;

    SPDLOG_DEBUG("Global UBO configured with {} point lights, {} spot lights, directional: {}",
        pointLightCount, spotLightCount, hasDirectionalLight);

    // Create and update uniform buffer using SmartHandle
    auto globalUboHandle = m_uniformBufferManager.acquireSmartBuffer(ShaderLib::GLOBAL_UBO);
    if (globalUboHandle.isValid()) {
        m_uniformBufferManager.updateBuffer(globalUboHandle.handle(), &globalUboData, sizeof(ShaderLib::GlobalUBOData));
        SPDLOG_DEBUG("Created and updated global uniform buffer for camera {}", cameraEntity.id);
    }
    else {
        SPDLOG_ERROR("Failed to acquire global uniform buffer for camera {}", cameraEntity.id);
    }

    return globalUboHandle;
}

std::vector<std::shared_ptr<LightRenderOrder>> CameraProcessingStage::cullLights(
    std::shared_ptr<CameraRenderOrder> cameraOrder,
    const std::vector<std::shared_ptr<LightRenderOrder>>& allLights) {

    // Current implementation: pass-through all lights
    // Future: implement frustum culling, distance culling, etc.

    SPDLOG_DEBUG("Light culling for camera {} - pass-through mode (no culling)",
        cameraOrder->entity.id);

    return allLights;
}