#pragma once
#include <memory>
#include <vector>
#include "ProcessingStage.h"
#include "Registry.h"
#include "AssetSystem.h"
#include "AssetResolutionStage.h"
#include "UniformBufferStage.h"
#include "RenderStage.h"
#include "Renderer.h"
#include "DescriptorSetStage.h"
#include "PipelineAssignmentStage.h"
#include "GlobalStateManager.h"

// Forward declarations
class AssetResolutionStage;
class UniformBufferStage;
class DescriptorSetStage;
class PipelineAssignmentStage;
class RenderStage;

class RenderSystem {
public:
    RenderSystem(Registry& registry, AssetSystem& assetSystem, Renderer& renderer);
    ~RenderSystem() = default;

    // Submit a render order to the pipeline
    void submitRenderOrder(std::shared_ptr<RenderOrder> order);

    // Submit a batch of render orders
    void submitRenderOrders(const std::vector<std::shared_ptr<RenderOrder>>& orders);

    // Process all pending render orders
    void processOrders();

    // Execute rendering commands for the current frame
    void renderFrame();

    // Reset the system for the next frame
    void prepareForNextFrame();

    void reset();
private:
    Registry& m_registry;
    AssetSystem& m_assetSystem;
    Renderer& m_renderer;

    std::unique_ptr<GlobalStateManager> m_globalStateManager;

    // Pipeline stages
    std::shared_ptr<AssetResolutionStage> m_assetResolutionStage;
    std::shared_ptr<UniformBufferStage> m_uniformBufferStage;
    std::shared_ptr<DescriptorSetStage> m_descriptorSetStage;
    std::shared_ptr<PipelineAssignmentStage> m_pipelineAssignmentStage;
    std::shared_ptr<RenderStage> m_renderStage;

    // Initialize the pipeline stages and their connections
    void initializePipeline();

    // Pending orders to be processed
    std::vector<std::shared_ptr<RenderOrder>> m_pendingOrders;
};