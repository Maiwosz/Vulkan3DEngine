#pragma once
#include "ProcessingStage.h"
#include "CameraRenderOrder.h"
#include "MeshRenderOrder.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include "AssetSystem.h"
#include <spdlog/spdlog.h>
#include <memory>
#include <vector>

// Forward declarations
class GpuCall;
class DrawCall;

/**
 * RenderStage - executes camera render orders using RenderGraphExecutor
 *
 * UPDATED ARCHITECTURE:
 * For each camera render order:
 * 1. Assign RenderGraph from camera order to renderer
 * 2. Begin frame
 * 3. Collect already prepared DrawCall commands from all culled meshes (created in AssetResolutionStage)
 * 4. Execute all DrawCalls through RenderGraphExecutor (which handles render pass management)
 * 5. End frame
 *
 * RenderGraphExecutor handles:
 * - Render graph traversal and execution
 * - Render pass lifecycle per node
 * - Framebuffer management
 * - Viewport/scissor setup
 *
 * DrawCall handles:
 * - Mesh geometry binding and drawing
 * - Validation of mesh data
 * - Pipeline configuration
 *
 * AssetResolutionStage handles:
 * - Creating and configuring DrawCall with mesh data
 */
class RenderStage : public ProcessingStage {
public:
    RenderStage(ProcessingContext& context, EngineCore& engineCore, AssetSystem& assetSystem);
    ~RenderStage() override = default;

    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

private:
    Renderer& m_renderer;
    AssetSystem& m_assetSystem;

    // Process camera render order using updated architecture
    ProcessingResult processCameraOrder(std::shared_ptr<CameraRenderOrder> cameraOrder);

    // Assign render graph from camera order to renderer
    bool assignRenderGraphToRenderer(const CameraRenderOrder& cameraOrder);

    // Collect already prepared DrawCall commands from all meshes in camera order
    std::vector<std::unique_ptr<GpuCall>> collectDrawCallsFromCamera(const CameraRenderOrder& cameraOrder);

    // Validation helpers
    bool validateCameraOrder(const CameraRenderOrder& cameraOrder) const;
    bool validateMeshOrder(const MeshRenderOrder& meshOrder) const;
};