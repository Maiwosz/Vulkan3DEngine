#pragma once
#include <memory>
#include <vector>
#include "ProcessingStage.h"
#include "Registry.h"
#include "AssetManager.h"
#include "OrderCollectionStage.h"
#include "AssetResolutionStage.h"

// Forward declarations
class OrderCollectionStage;
class AssetResolutionStage;

class RenderSystem {
public:
    RenderSystem(Registry& registry, AssetManager& assetManager);
    ~RenderSystem() = default;

    // Submit a render order to the pipeline
    void submitRenderOrder(std::shared_ptr<RenderOrder> order);

    // Submit a batch of render orders
    void submitRenderOrders(const std::vector<std::shared_ptr<RenderOrder>>& orders);

    // Process all pending render orders
    void processOrders();

    // Reset the system for the next frame
    void prepareForNextFrame();

private:
    Registry& m_registry;
    AssetManager& m_assetManager;

    // Pipeline stages
    std::shared_ptr<OrderCollectionStage> m_collectionStage;
    std::shared_ptr<AssetResolutionStage> m_assetResolutionStage;

    // Initialize the pipeline stages and their connections
    void initializePipeline();

    // Pending orders to be processed
    std::vector<std::shared_ptr<RenderOrder>> m_pendingOrders;
};