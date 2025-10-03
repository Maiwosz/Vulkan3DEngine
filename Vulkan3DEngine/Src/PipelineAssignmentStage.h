#pragma once
#include "AssetSystem.h"
#include "ProcessingStage.h"
#include "RenderPassManager.h"
#include "ShaderModuleManager.h"
#include "Handle.h"
#include "EngineCore.h"

// Forward declarations
class MaterialManager;
class MeshRenderOrder;

class PipelineAssignmentStage : public ProcessingStage {
public:
    PipelineAssignmentStage(
        ProcessingContext& context,
        EngineCore& renderer,
        AssetSystem& assetSystem,
        Settings& settings
    );

    ~PipelineAssignmentStage();

    // Process a single render order - only accepts MeshRenderOrder
    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

private:
    // Process mesh render order and assign pipeline config to its draw call
    ProcessingResult processMeshOrder(std::shared_ptr<MeshRenderOrder> meshOrder);

    // Create a pipeline configuration for the material and mesh
    GraphicsPipelineConfig createPipelineConfig(
        MaterialHandle materialHandle,
        const MeshHandle& mesh
    );

    // Create vertex input configuration based on mesh format
    VertexInputConfig createVertexInputConfig(const Mesh& mesh);

    // References to required managers
    Settings& m_settings;
    ShaderManager& m_shaderManager;
    MaterialManager& m_materialManager;
    RenderPassManager& m_renderPassManager;
    MeshManager& m_meshManager;

    // Default render pass handle for when one isn't specified
    RenderPassHandle m_defaultRenderPassHandle;
};