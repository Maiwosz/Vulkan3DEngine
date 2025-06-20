#pragma once
#include "AssetSystem.h"
#include "ProcessingStage.h"
#include "PipelineManager.h"
#include "RenderPassManager.h"
#include "ShaderModuleManager.h"
#include "Handle.h"
#include "Renderer.h"

// Forward declarations
class MaterialManager;

class PipelineAssignmentStage : public OrderProcessingStage {
public:
    PipelineAssignmentStage(
        Renderer& renderer,
        AssetSystem& assetSystem,
        Settings& settings
    );

    ~PipelineAssignmentStage();

    // Process a single render order
    void process(std::shared_ptr<RenderOrder> order) override;

    // Pipeline creation
    PipelineHandle getPipelineForMaterialAndMesh(
        MaterialHandle materialHandle,
        const MeshHandle& meshHandle,
        RenderPassHandle renderPassHandle
    );

private:
    // Create a pipeline configuration for the material and mesh
    GraphicsPipelineConfig createPipelineConfig(
        MaterialHandle materialHandle,
        const Mesh& mesh,
        RenderPassHandle renderPassHandle
    );

    // Create vertex input configuration based on mesh format
    VertexInputConfig createVertexInputConfig(const Mesh& mesh);

    // References to required managers
    Settings& m_settings;
    PipelineManager& m_pipelineManager;
    ShaderManager& m_shaderManager;
    MaterialManager& m_materialManager;
    RenderPassManager& m_renderPassManager;
    MeshManager& m_meshManager;

    // Default render pass handle for when one isn't specified
    // This will be used temporarily until proper render pass handling is implemented
    RenderPassHandle m_defaultRenderPassHandle;
};