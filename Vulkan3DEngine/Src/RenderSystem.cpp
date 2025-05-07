#include "RenderSystem.h"

RenderSystem::RenderSystem(Registry& registry, AssetManager& assetManager, Renderer& renderer)
    : m_registry(registry), m_assetManager(assetManager), m_renderer(renderer) {
    initializePipeline();
}

void RenderSystem::initializePipeline() {
    // Create pipeline stages
    m_collectionStage = std::make_shared<OrderCollectionStage>();
    m_assetResolutionStage = std::make_shared<AssetResolutionStage>(m_registry, m_assetManager);
    m_uniformBufferStage = std::make_shared<UniformBufferStage>(m_registry, m_renderer);
    m_descriptorSetStage = std::make_shared<DescriptorSetStage>(m_renderer);
    m_pipelineAssignmentStage = std::make_shared<PipelineAssignmentStage>(m_renderer);
    m_renderStage = std::make_shared<RenderStage>(m_renderer);

    // Connect stages - Entry point for all render orders
    m_collectionStage->connectTo(RenderOrderType::Mesh, m_assetResolutionStage);
    m_assetResolutionStage->connectTo(RenderOrderType::Mesh, m_uniformBufferStage);

    // Camera and Light orders skip asset resolution stage
    m_collectionStage->connectTo(RenderOrderType::Camera, m_uniformBufferStage);
    m_collectionStage->connectTo(RenderOrderType::Light, m_uniformBufferStage);

    // Connect UniformBufferStage to DescriptorSetStage for all order types
    m_uniformBufferStage->connectTo(RenderOrderType::Mesh, m_descriptorSetStage);
    m_uniformBufferStage->connectTo(RenderOrderType::Camera, m_descriptorSetStage);
    m_uniformBufferStage->connectTo(RenderOrderType::Light, m_descriptorSetStage);

    // Connect DescriptorSetStage to PipelineAssignmentStage
    m_descriptorSetStage->connectTo(RenderOrderType::Mesh, m_pipelineAssignmentStage);
    m_descriptorSetStage->connectTo(RenderOrderType::Camera, m_pipelineAssignmentStage);
    m_descriptorSetStage->connectTo(RenderOrderType::Light, m_pipelineAssignmentStage);

    // Connect PipelineAssignmentStage to RenderStage (final step)
    m_pipelineAssignmentStage->connectTo(RenderOrderType::Mesh, m_renderStage);
    m_pipelineAssignmentStage->connectTo(RenderOrderType::Camera, m_renderStage);
    m_pipelineAssignmentStage->connectTo(RenderOrderType::Light, m_renderStage);

}

void RenderSystem::submitRenderOrder(std::shared_ptr<RenderOrder> order) {
    m_pendingOrders.push_back(std::move(order));
}

void RenderSystem::submitRenderOrders(const std::vector<std::shared_ptr<RenderOrder>>& orders) {
    m_pendingOrders.insert(m_pendingOrders.end(), orders.begin(), orders.end());
}

void RenderSystem::processOrders() {
    if (m_pendingOrders.empty()) {
        return;
    }
    try {
        // Process all pending orders through the pipeline
        m_collectionStage->processBatch(m_pendingOrders);

        m_pendingOrders.clear();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error processing render orders: {}", e.what());
        // Clear pending orders on error
        m_pendingOrders.clear();
        throw; // Rethrow to be handled by engine
    }
}

void RenderSystem::prepareForNextFrame() {
    // Reset all stages that maintain state between frames
    if (m_uniformBufferStage) {
        m_uniformBufferStage->reset();
    }

    // Reset any per-frame state
    resetForNextFrame();

    // Additional frame preparation logic can be added here
}

void RenderSystem::resetForNextFrame() {
    // Clear current frame data
    m_renderer.frameManager().clearCurrentFrameOrders();
    m_pendingOrders.clear();
}

void RenderSystem::renderFrame() {
    try {
        m_renderStage->createAndRenderDebugObject();

        // Execute the actual render commands
        m_renderStage->executeRenderPass();

        // Prepare for the next frame
        prepareForNextFrame();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error rendering frame: {}", e.what());
        // Attempt to reset for next frame even if rendering failed
        resetForNextFrame();
        throw; // Rethrow to be handled by engine
    }
}