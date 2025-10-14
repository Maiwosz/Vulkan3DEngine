#pragma once
#include "ProcessingStage.h"
#include "Handle.h"

// Forward declarations
class EngineCore;
class CameraRenderOrder;
class Registry;
class AssetSystem;

class RenderPipelineAssignmentStage : public ProcessingStage {
public:
    RenderPipelineAssignmentStage(ProcessingContext& context, AssetSystem& assetSystem, EngineCore& engineCore, Registry& registry);
    ~RenderPipelineAssignmentStage() override = default;

    // ProcessingStage interface
    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

private:
    AssetSystem& m_assetSystem;
    EngineCore& m_engineCore;
    Registry& m_registry;

    // Helper method to process camera orders specifically
    ProcessingResult processCameraOrder(std::shared_ptr<CameraRenderOrder> cameraOrder);
};