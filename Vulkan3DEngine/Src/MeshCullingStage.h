#pragma once
#include "ProcessingStage.h"
#include "CameraRenderOrder.h"
#include "MeshRenderOrder.h"
#include "Registry.h"
#include <memory>
#include <vector>

// Forward declarations
struct TransformComponent;
struct CameraComponent;

class MeshCullingStage : public ProcessingStage {
public:
    MeshCullingStage(ProcessingContext& context, Registry& registry);
    ~MeshCullingStage() override = default;

    // Process camera render orders and perform mesh culling
    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

private:
    // Perform mesh culling for a specific camera
    ProcessingResult cullMeshesForCamera(std::shared_ptr<CameraRenderOrder> cameraOrder);

    // Culling algorithms (currently pass-through)
    std::vector<std::shared_ptr<MeshRenderOrder>> performFrustumCulling(
        std::shared_ptr<CameraRenderOrder> cameraOrder,
        const std::vector<std::shared_ptr<MeshRenderOrder>>& meshes);

    Registry& m_registry;
};
