#pragma once
#include <memory>
#include <vector>
#include "ProcessingPipeline.h"
#include "ProcessingContext.h"
#include "SemaphoreManager.h"
#include "Registry.h"
#include "AssetSystem.h"
#include "EngineCore.h"
#include "Settings.h"
#include "RenderOrder.h"

// Stage includes
#include "AssetResolutionStage.h"
#include "UniformBufferStage.h"
#include "DescriptorSetStage.h"
#include "MeshStorageStage.h"
#include "LightStorageStage.h"
#include "CameraProcessingStage.h"
#include "RenderPipelineAssignmentStage.h"
#include "MeshCullingStage.h"
#include "PipelineAssignmentStage.h"
#include "RenderStage.h"
#include "SynchronizationStages.h"

class RenderSystem {
public:
    RenderSystem(Registry& registry, AssetSystem& assetSystem, EngineCore& renderer, Settings& settings);
    ~RenderSystem() = default;

    // Non-copyable, non-movable
    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;
    RenderSystem(RenderSystem&&) = delete;
    RenderSystem& operator=(RenderSystem&&) = delete;

    // Submit a render order to the pipeline
    // Now also stores it in current FrameData
    void submitRenderOrder(std::shared_ptr<RenderOrder> order);

    // Submit a batch of render orders
    // Now also stores them in current FrameData
    void submitRenderOrders(const std::vector<std::shared_ptr<RenderOrder>>& orders);

    // Process all pending render orders
    void processOrders();

    // Reset the system for the next frame
    void prepareForNextFrame();

    void reset();

    // Access to semaphore manager for debugging
    const SemaphoreManager& getSemaphoreManager() const { return *m_semaphoreManager; }

private:
    Settings& m_settings;
    Registry& m_registry;
    AssetSystem& m_assetSystem;
    EngineCore& m_renderer;

    // Semaphore manager - owned by RenderSystem
    std::unique_ptr<SemaphoreManager> m_semaphoreManager;

    // Processing context for cross-pipeline communication
    std::unique_ptr<ProcessingContext> m_processingContext;

    // Processing pipelines
    std::unique_ptr<ProcessingPipeline> m_meshPipeline;
    std::unique_ptr<ProcessingPipeline> m_lightPipeline;
    std::unique_ptr<ProcessingPipeline> m_cameraPipeline;

    // === OWNED PROCESSING STAGES ===
    // Asset processing stages
    std::shared_ptr<AssetResolutionStage> m_assetResolutionStage;

    // Buffer and descriptor stages
    std::shared_ptr<UniformBufferStage> m_uniformBufferStage;
    std::shared_ptr<DescriptorSetStage> m_descriptorSetStage;

    // Storage stages
    std::shared_ptr<MeshStorageStage> m_meshStorageStage;
    std::shared_ptr<LightStorageStage> m_lightStorageStage;

    // Camera processing stages
    std::shared_ptr<CameraProcessingStage> m_cameraProcessingStage;
    std::shared_ptr<RenderPipelineAssignmentStage> m_renderPipelineAssignmentStage;
    std::shared_ptr<MeshCullingStage> m_meshCullingStage;

    // Pipeline assignment stage
    std::shared_ptr<PipelineAssignmentStage> m_pipelineAssignmentStage;

    // Final rendering stage
    std::shared_ptr<RenderStage> m_renderStage;

    // Synchronization stages
    std::shared_ptr<WaitForStage> m_waitForLightsStage;
    std::shared_ptr<WaitForStage> m_waitForMeshesStage;
    std::shared_ptr<NotifyStage> m_notifyLightsStoredStage;
    std::shared_ptr<NotifyStage> m_notifyMeshesStoredStage;

    // Initialize all processing stages (called once)
    void initializeStages();

    // Initialize the processing pipelines using owned stages
    void initializePipelines();

    // Create mesh processing pipeline
    void createMeshPipeline();

    // Create light processing pipeline  
    void createLightPipeline();

    // Create camera processing pipeline
    void createCameraPipeline();

    // Pending orders to be processed
    std::vector<std::shared_ptr<RenderOrder>> m_pendingOrders;

    // Separate orders by type for pipeline processing
    void categorizeOrders(
        std::vector<std::shared_ptr<RenderOrder>>& meshOrders,
        std::vector<std::shared_ptr<RenderOrder>>& lightOrders,
        std::vector<std::shared_ptr<RenderOrder>>& cameraOrders,
        std::vector<std::shared_ptr<RenderOrder>>& otherOrders);

    // Helper method to add order to current frame
    void addOrderToCurrentFrame(std::shared_ptr<RenderOrder> order);
};
