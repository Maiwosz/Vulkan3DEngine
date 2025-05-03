#include "RenderSystem.h"

RenderSystem::RenderSystem(Registry& registry, AssetManager& assetManager)
    : m_registry(registry), m_assetManager(assetManager) {
    initializePipeline();
}

void RenderSystem::initializePipeline() {
    // Create pipeline stages
    m_collectionStage = std::make_shared<OrderCollectionStage>();
    m_assetResolutionStage = std::make_shared<AssetResolutionStage>(m_registry, m_assetManager);

    // Connect stages - Entry point for all render orders
    m_collectionStage->connectTo(RenderOrderType::Mesh, m_assetResolutionStage);

    // Camera and Light orders skip asset resolution stage
    m_collectionStage->connectTo(RenderOrderType::Camera, nullptr); // Will connect to uniform buffer stage later
    m_collectionStage->connectTo(RenderOrderType::Light, nullptr);  // Will connect to uniform buffer stage later

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

    // Process all pending orders through the pipeline
    m_collectionStage->processBatch(m_pendingOrders);

    // Clear pending orders after processing
    m_pendingOrders.clear();
}

void RenderSystem::prepareForNextFrame() {
    // Reset any per-frame state
    m_pendingOrders.clear();

    // Additional frame preparation logic can be added here
}