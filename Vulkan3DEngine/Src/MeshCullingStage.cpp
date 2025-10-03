#include "MeshCullingStage.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include <spdlog/spdlog.h>

MeshCullingStage::MeshCullingStage(ProcessingContext& context, Registry& registry)
    : ProcessingStage(context), m_registry(registry)
{
    SPDLOG_INFO("Initialized MeshCullingStage");
}

ProcessingResult MeshCullingStage::process(std::shared_ptr<RenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("MeshCullingStage received null render order");
        return ProcessingResult::Failure;
    }

    if (order->getType() != RenderOrderType::Camera) {
        SPDLOG_DEBUG("MeshCullingStage skipping non-camera order type: {}",
            renderOrderTypeToString(order->getType()));
        return ProcessingResult::Failure; // Not an error, but this order shouldn't be processed by this stage
    }

    auto cameraOrder = std::static_pointer_cast<CameraRenderOrder>(order);
    return cullMeshesForCamera(cameraOrder);
}

ProcessingResult MeshCullingStage::cullMeshesForCamera(std::shared_ptr<CameraRenderOrder> cameraOrder) {
    SPDLOG_DEBUG("Performing mesh culling for camera entity: {}", cameraOrder->entity.id);

    // Get all processed meshes from context
    const auto& allMeshes = m_context.getProcessedMeshes();

    if (allMeshes.empty()) {
        SPDLOG_DEBUG("No meshes available for culling for camera: {}", cameraOrder->entity.id);
        // This is not necessarily an error - there might simply be no meshes to render
        cameraOrder->culledMeshes.clear();
        return ProcessingResult::Success;
    }

    // Perform frustum culling
    auto culledMeshes = performFrustumCulling(cameraOrder, allMeshes);

    // Store culled meshes in camera order
    cameraOrder->culledMeshes = culledMeshes;

    SPDLOG_DEBUG("Camera {} culled {} meshes from {} total",
        cameraOrder->entity.id, culledMeshes.size(), allMeshes.size());

    return ProcessingResult::Success;
}

std::vector<std::shared_ptr<MeshRenderOrder>> MeshCullingStage::performFrustumCulling(
    std::shared_ptr<CameraRenderOrder> cameraOrder,
    const std::vector<std::shared_ptr<MeshRenderOrder>>& meshes) {

    // Current implementation: pass-through all meshes
    // Future: implement proper frustum culling using camera frustum and mesh bounds

    Entity cameraEntity = cameraOrder->entity;

    if (!m_registry.components().hasComponent<CameraComponent>(cameraEntity) ||
        !m_registry.components().hasComponent<TransformComponent>(cameraEntity)) {
        SPDLOG_WARN("Camera entity {} missing required components for culling", cameraEntity.id);
        return meshes; // Return all meshes as fallback
    }

    // Get camera components for future culling implementation
    auto& cameraComponent = m_registry.components().getComponent<CameraComponent>(cameraEntity);
    auto& cameraTransform = m_registry.components().getComponent<TransformComponent>(cameraEntity);

    SPDLOG_DEBUG("Frustum culling for camera {} - pass-through mode (no culling)",
        cameraEntity.id);

    // TODO: Implement actual frustum culling:
    // 1. Extract frustum planes from camera view-projection matrix
    // 2. Test each mesh's bounding box/sphere against frustum
    // 3. Return only visible meshes

    return meshes; // Pass-through for now
}