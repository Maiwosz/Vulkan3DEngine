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

    // 1. Assign the render graph from camera order to renderer
    if (!assignRenderGraphToRenderer(*cameraOrder)) {
        SPDLOG_ERROR("Failed to assign render graph to renderer for camera: {}", cameraOrder->entity.id);
        return ProcessingResult::Failure;
    }

    // 2. Begin frame - Renderer handles frame setup
    if (!m_renderer.beginFrame()) {
        SPDLOG_ERROR("Failed to begin frame for camera: {}", cameraOrder->entity.id);
        return ProcessingResult::Failure;
    }

    try {
        // 3. Collect DrawCall commands from all culled meshes (they're already prepared)
        std::vector<std::unique_ptr<GpuCall>> drawCalls = collectDrawCallsFromCamera(*cameraOrder);

        if (drawCalls.empty()) {
            SPDLOG_DEBUG("No draw calls found for camera: {}", cameraOrder->entity.id);
            m_renderer.endFrame();
            return ProcessingResult::Success;
        }

        // 4. Execute all draw calls through RenderGraphExecutor (now using assigned graph)
        bool success = m_renderer.executeRenderGraph(drawCalls);

        if (!success) {
            SPDLOG_ERROR("Failed to execute render graph for camera: {}", cameraOrder->entity.id);
            m_renderer.endFrame();
            return ProcessingResult::Failure;
        }

        // 5. End frame - Renderer handles cleanup and presentation
        m_renderer.endFrame();
        return ProcessingResult::Success;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during camera rendering: {}", e.what());
        m_renderer.endFrame(); // Cleanup on error
        return ProcessingResult::Failure;
    }
}

bool RenderStage::assignRenderGraphToRenderer(const CameraRenderOrder& cameraOrder) {
    if (!cameraOrder.hasValidRenderGraph()) {
        SPDLOG_ERROR("Camera order has invalid render graph: {}", cameraOrder.entity.id);
        return false;
    }

    // Assign the render graph to the renderer
    m_renderer.assignRenderGraph(cameraOrder.renderGraphHandle);

    SPDLOG_DEBUG("Assigned render graph (ID: {}) to renderer for camera: {}",
        cameraOrder.renderGraphHandle.handle().id, cameraOrder.entity.id);

    return true;
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

                SPDLOG_DEBUG("Collected DrawCall for mesh entity",
                    meshOrder->entity.id);
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
    // Updated validation - now also checks if DrawCall has mesh data
    return meshOrder.isReadyForRendering();
}