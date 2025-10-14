#include "RenderPipelineAssignmentStage.h"
#include "CameraRenderOrder.h"
#include "CameraComponent.h"
#include "EngineCore.h"
#include "Registry.h"
#include "BuiltInGraphTemplates.h"
#include "AssetSystem.h"
#include <spdlog/spdlog.h>

RenderPipelineAssignmentStage::RenderPipelineAssignmentStage(
    ProcessingContext& context,
    AssetSystem& assetSystem,
    EngineCore& engineCore,
    Registry& registry)
    : ProcessingStage(context), m_assetSystem(assetSystem), m_engineCore(engineCore), m_registry(registry) {
    SPDLOG_DEBUG("RenderPipelineAssignmentStage created");
}

ProcessingResult RenderPipelineAssignmentStage::process(std::shared_ptr<RenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("RenderPipelineAssignmentStage::process() - null order received");
        return ProcessingResult::Failure;
    }

    // Process only Camera render orders
    if (order->getType() == RenderOrderType::Camera) {
        auto cameraOrder = std::static_pointer_cast<CameraRenderOrder>(order);
        return processCameraOrder(cameraOrder);
    }

    // For other types, log warning and return false
    SPDLOG_WARN("RenderPipelineAssignmentStage: Unsupported render order type: {}",
        renderOrderTypeToString(order->getType()));
    return ProcessingResult::Failure;
}

ProcessingResult RenderPipelineAssignmentStage::processCameraOrder(std::shared_ptr<CameraRenderOrder> cameraOrder) {
    if (!cameraOrder) {
        SPDLOG_ERROR("RenderPipelineAssignmentStage::processCameraOrder() - null camera order");
        return ProcessingResult::Failure;
    }

    try {
        // Get the camera component to access its render target
        auto& cameraComponent = m_registry.components().getComponent<CameraComponent>(cameraOrder->entity);
        const RenderTarget& renderTarget = cameraComponent.getRenderTarget();

        SPDLOG_DEBUG("RenderPipelineAssignmentStage: Processing camera entity {} with {} render target",
            cameraOrder->entity.id,
            renderTarget.isSwapchain() ? "swapchain" : "texture");

        // Get the template manager and acquire smart handle to forward rendering template
        auto& templateManager = m_assetSystem.renderGraphTemplateManager();
        auto templateHandle = templateManager.getTemplateSmartHandle("ForwardRendering");

        if (!templateHandle.isValid()) {
            SPDLOG_ERROR("RenderPipelineAssignmentStage: Failed to get ForwardRendering template for camera entity {}",
                cameraOrder->entity.id);
            return ProcessingResult::Failure;
        }

        // Acquire the appropriate render graph for this render target
        auto renderGraphHandle = m_engineCore.renderGraphManager().acquireSmartRenderGraph(
            templateHandle,
            renderTarget
        );

        if (!renderGraphHandle.isValid()) {
            SPDLOG_ERROR("RenderPipelineAssignmentStage: Failed to acquire render graph for camera entity {}",
                cameraOrder->entity.id);
            return ProcessingResult::Failure;
        }

        SPDLOG_DEBUG("RenderPipelineAssignmentStage: Assigned render graph (ID: {}) to camera entity {}",
            renderGraphHandle.handle().id, cameraOrder->entity.id);

        // Assign the render graph to the camera order
        cameraOrder->renderGraphHandle = renderGraphHandle;

        SPDLOG_INFO("RenderPipelineAssignmentStage: Successfully assigned render graph {} to camera {}",
            renderGraphHandle.handle().id, cameraOrder->entity.id);

        return ProcessingResult::Success;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("RenderPipelineAssignmentStage: Exception while processing camera order for entity {}: {}",
            cameraOrder->entity.id, e.what());
        return ProcessingResult::Failure;
    }
}