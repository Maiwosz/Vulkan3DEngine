#include "RenderStage.h"
#include "Mesh.h"
#include "Material.h"
#include "DrawCall.h"
#include "MeshRenderOrder.h"
#include "RenderGraph.h"
#include <spdlog/spdlog.h>

RenderStage::RenderStage(ProcessingContext& context, EngineCore& engineCore, AssetSystem& assetSystem)
    : ProcessingStage(context), m_renderer(engineCore.renderer()), m_assetSystem(assetSystem)
{
    SPDLOG_DEBUG("RenderStage initialized");
}

ProcessingResult RenderStage::process(std::shared_ptr<RenderOrder> order) {
    if (!order || order->getType() != RenderOrderType::Camera) {
        return ProcessingResult::Success; // Skip non-camera orders
    }

    auto cameraOrder = std::static_pointer_cast<CameraRenderOrder>(order);
    return processCameraOrder(cameraOrder);
}

ProcessingResult RenderStage::processCameraOrder(std::shared_ptr<CameraRenderOrder> cameraOrder) {
    if (!validateCameraOrder(*cameraOrder)) {
        return ProcessingResult::Failure;
    }

    // Collect DrawCall commands from all culled meshes
    std::vector<std::unique_ptr<GpuCall>> drawCalls = collectDrawCallsFromCamera(*cameraOrder);

    if (drawCalls.empty()) {
        SPDLOG_DEBUG("No draw calls found for camera: {}", cameraOrder->entity.id);

        // Render empty frame to maintain frame pacing
        if (!m_renderer.renderEmptyFrame()) {
            SPDLOG_ERROR("Failed to render empty frame for camera: {}", cameraOrder->entity.id);
            return ProcessingResult::Failure;
        }

        return ProcessingResult::Success;
    }

    // SIMPLIFIED: Single call to render complete frame
    bool success = m_renderer.renderFrame(cameraOrder->renderGraphHandle, drawCalls);

    if (!success) {
        SPDLOG_ERROR("Failed to render frame for camera: {}", cameraOrder->entity.id);
        return ProcessingResult::Failure;
    }

    SPDLOG_DEBUG("Successfully rendered frame for camera: {} ({} draw calls)",
        cameraOrder->entity.id, drawCalls.size());

    return ProcessingResult::Success;
}

std::vector<std::unique_ptr<GpuCall>> RenderStage::collectDrawCallsFromCamera(const CameraRenderOrder& cameraOrder) {
    std::vector<std::unique_ptr<GpuCall>> drawCalls;
    drawCalls.reserve(cameraOrder.culledMeshes.size());

    SPDLOG_DEBUG("Collecting {} draw calls from camera: {}",
        cameraOrder.culledMeshes.size(), cameraOrder.entity.id);

    for (const auto& meshOrder : cameraOrder.culledMeshes) {
        if (!meshOrder) {
            continue;
        }

        try {
            // Use the already prepared DrawCall from MeshRenderOrder
            if (meshOrder->drawCall && meshOrder->drawCall->hasMeshData()) {
                meshOrder->drawCall->setGlobalDescriptorSet(cameraOrder.globalDescriptorSetHandle);

                drawCalls.push_back(std::move(meshOrder->drawCall));

                SPDLOG_DEBUG("Collected DrawCall for mesh entity: {}", meshOrder->entity.id);
            }
            else {
                SPDLOG_WARN("Mesh order has no valid DrawCall for entity: {}", meshOrder->entity.id);
            }
        }
        catch (const std::exception& e) {
            SPDLOG_WARN("Failed to collect draw call for mesh entity {}: {}",
                meshOrder->entity.id, e.what());
            // Continue with other meshes
        }
    }

    SPDLOG_DEBUG("Collected {} draw calls successfully", drawCalls.size());
    return drawCalls;
}

bool RenderStage::validateCameraOrder(const CameraRenderOrder& cameraOrder) const {
    if (!cameraOrder.isReadyForRendering()) {
        SPDLOG_ERROR("Camera order not ready: {}", cameraOrder.entity.id);
        return false;
    }

    // Check if camera has valid render graph
    if (!cameraOrder.hasValidRenderGraph()) {
        SPDLOG_ERROR("No render graph assigned to camera order: {}", cameraOrder.entity.id);
        return false;
    }

    return true;
}

bool RenderStage::validateMeshOrder(const MeshRenderOrder& meshOrder) const {
    return meshOrder.isReadyForRendering();
}