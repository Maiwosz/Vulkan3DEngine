#pragma once
#include "AssetSystem.h"
#include "ProcessingStage.h"
#include "PipelineManager.h"
#include "RenderPassManager.h"
#include "ShaderModuleManager.h"
#include "Handle.h"
#include <unordered_map>
#include "Renderer.h"

// Forward declarations
class MaterialManager;

// Structure to hold pipeline configuration for a specific material/mesh combination
struct MaterialMeshPipelineKey {
    MaterialHandle materialHandle;
    uint32_t meshVertexFormat; // From VramMesh vertex format

    bool operator==(const MaterialMeshPipelineKey& other) const {
        return materialHandle == other.materialHandle &&
            meshVertexFormat == other.meshVertexFormat;
    }
};

// Custom hash function for the MaterialMeshPipelineKey
namespace std {
    template<>
    struct hash<MaterialMeshPipelineKey> {
        size_t operator()(const MaterialMeshPipelineKey& key) const {
            // Combine material handle hash with vertex format
            size_t seed = 0;
            hash_combine(seed, key.materialHandle);
            hash_combine(seed, key.meshVertexFormat);
            return seed;
        }
    };
}

class PipelineAssignmentStage : public OrderProcessingStage {
public:
    PipelineAssignmentStage(
        Renderer& renderer,
		AssetSystem& assetSystem
    );

    ~PipelineAssignmentStage();

    // Process a single render order
    void process(std::shared_ptr<RenderOrder> order) override;

    // Pipeline creation/caching
    PipelineHandle getPipelineForMaterialAndMesh(
        MaterialHandle materialHandle,
        const MeshHandle& meshHandle,
        RenderPassHandle renderPassHandle
    );

    // Clear pipeline cache
    void clearCache();

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
    PipelineManager& m_pipelineManager;
    ShaderManager& m_shaderManager;
    MaterialManager& m_materialManager;
    RenderPassManager& m_renderPassManager;
    MeshManager& m_meshManager;

    // Cache for pipelines to avoid recreation
    struct PipelineCacheEntry {
        PipelineHandle pipelineHandle;
        RenderPassHandle renderPassHandle;
    };

    std::unordered_map<MaterialMeshPipelineKey, PipelineCacheEntry> m_pipelineCache;

    // Default render pass handle for when one isn't specified
    // This will be used temporarily until proper render pass handling is implemented
    RenderPassHandle m_defaultRenderPassHandle;
};