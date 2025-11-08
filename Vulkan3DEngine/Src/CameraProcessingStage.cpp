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
    m_bufferManager(renderer.bufferManager()),
    m_firstFrame(true)
{
    // Create and cache definitions
    m_globalUBODef = ShaderLib::CreateGlobalUBODefinition();
    m_directionalLightDef = ShaderLib::CreateDirectionalLightType();
    m_pointLightDef = ShaderLib::CreatePointLightType();
    m_spotLightDef = ShaderLib::CreateSpotLightType();

    // Create reusable instance - we'll swap mapped buffer each frame
    m_cachedInstance = m_globalUBODef->CreateInstance();

    // Cache all field offsets for O(1) direct memory access
    const auto* viewField = m_globalUBODef->FindField("view");
    const auto* projField = m_globalUBODef->FindField("proj");
    const auto* cameraPosField = m_globalUBODef->FindField("cameraPosition");
    const auto* activePointLightsField = m_globalUBODef->FindField("activePointLights");

    // Directional light fields
    const auto* dirLightDirField = m_globalUBODef->FindField("directionalLight.direction");
    const auto* dirLightColorField = m_globalUBODef->FindField("directionalLight.color");

    // Point lights array - get first element to determine stride
    const auto* pointLights = m_globalUBODef->FindField("pointLights[0]");

    // Point light fields (relative offsets within each array element)
    const auto* pointLight0Pos = m_globalUBODef->FindField("pointLights[0].position");
    const auto* pointLight0Radius = m_globalUBODef->FindField("pointLights[0].radius");
    const auto* pointLight0Color = m_globalUBODef->FindField("pointLights[0].color");

    // Validate all required fields exist
    if (!viewField || !projField || !cameraPosField || !activePointLightsField ||
        !dirLightDirField || !dirLightColorField || !pointLights||
        !pointLight0Pos || !pointLight0Radius || !pointLight0Color) {
        throw std::runtime_error("Failed to find required fields in global UBO definition");
    }

    // Store absolute offsets
    m_offsets.view = viewField->offset;
    m_offsets.proj = projField->offset;
    m_offsets.cameraPosition = cameraPosField->offset;
    m_offsets.activePointLights = activePointLightsField->offset;
    m_offsets.dirLight_direction = dirLightDirField->offset;
    m_offsets.dirLight_color = dirLightColorField->offset;

    // Calculate array stride and base
    m_offsets.pointLights_base = pointLights->offset;
    m_offsets.pointLights_stride = pointLights->stride;

    // Calculate relative offsets within array element
    m_offsets.pointLight_position_rel = pointLight0Pos->relativeOffset;
    m_offsets.pointLight_radius_rel = pointLight0Radius->relativeOffset;
    m_offsets.pointLight_color_rel = pointLight0Color->relativeOffset;

    SPDLOG_INFO("Initialized CameraProcessingStage with cached instance and field offsets");
    SPDLOG_DEBUG("Camera field offsets - view: {}, proj: {}, cameraPos: {}",
        m_offsets.view, m_offsets.proj, m_offsets.cameraPosition);
    SPDLOG_DEBUG("Point lights - base: {}, stride: {}, pos_rel: {}, radius_rel: {}, color_rel: {}",
        m_offsets.pointLights_base, m_offsets.pointLights_stride,
        m_offsets.pointLight_position_rel, m_offsets.pointLight_radius_rel, m_offsets.pointLight_color_rel);
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
    const std::vector<std::shared_ptr<LightRenderOrder>>& lights) {

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

    // Get raw buffer for direct memory writes
    uint8_t* rawBuffer = m_cachedInstance->GetRawBuffer();

    // OPTIMIZATION: Direct memory writes using cached offsets!

    // Write camera matrices
    glm::mat4 viewMatrix = cameraTransform.getViewMatrix();
    glm::mat4 projMatrix = cameraComponent.getProjectionMatrix();

    // Apply Vulkan coordinate correction
    glm::mat4 vulkanCorrection = glm::mat4(1.0f);
    vulkanCorrection[1][1] = -1.0f;
    glm::mat4 correctedProj = vulkanCorrection * projMatrix;

    std::memcpy(rawBuffer + m_offsets.view, &viewMatrix, sizeof(glm::mat4));
    std::memcpy(rawBuffer + m_offsets.proj, &correctedProj, sizeof(glm::mat4));

    glm::vec3 cameraPos = cameraTransform.getPosition();
    std::memcpy(rawBuffer + m_offsets.cameraPosition, &cameraPos, sizeof(glm::vec3));

    // Process lights with direct memory writes
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
            // Write directional light directly
            glm::vec3 direction = glm::normalize(lightComponent.direction);
            glm::vec4 color = lightComponent.getColorWithIntensity();

            std::memcpy(rawBuffer + m_offsets.dirLight_direction, &direction, sizeof(glm::vec3));
            std::memcpy(rawBuffer + m_offsets.dirLight_color, &color, sizeof(glm::vec4));

            hasDirectionalLight = true;
        }
        else if (lightComponent.type == LightComponent::Type::Point && pointLightCount < 64) {
            // Calculate offset for this array element
            uint32_t elementOffset = m_offsets.pointLights_base +
                (pointLightCount * m_offsets.pointLights_stride);

            // Write point light data directly
            glm::vec3 position = lightTransform.getPosition();
            float radius = lightComponent.radius;
            glm::vec4 color = lightComponent.getColorWithIntensity();

            std::memcpy(rawBuffer + elementOffset + m_offsets.pointLight_position_rel,
                &position, sizeof(glm::vec3));
            std::memcpy(rawBuffer + elementOffset + m_offsets.pointLight_radius_rel,
                &radius, sizeof(float));
            std::memcpy(rawBuffer + elementOffset + m_offsets.pointLight_color_rel,
                &color, sizeof(glm::vec4));

            SPDLOG_DEBUG("Added point light {} to global UBO, entity: {}, position: ({}, {}, {}), radius: {}, color: ({}, {}, {}, {})",
                pointLightCount, lightEntity.id,
                position.x, position.y, position.z, radius,
                color.r, color.g, color.b, color.a);

            pointLightCount++;
        }
    }

    // Write active light count
    std::memcpy(rawBuffer + m_offsets.activePointLights, &pointLightCount, sizeof(int));

    // Synchronize to GPU
    try {
        m_cachedInstance->SyncToBuffer();
        globalUboHandle->unmap();
        SPDLOG_DEBUG("Successfully synchronized global UBO data to GPU for camera {}", cameraEntity.id);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to sync global UBO to buffer: {}", e.what());
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
