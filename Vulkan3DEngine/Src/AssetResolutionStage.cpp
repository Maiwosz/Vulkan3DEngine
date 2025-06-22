#include "AssetResolutionStage.h"

AssetResolutionStage::AssetResolutionStage(Registry& registry, AssetSystem& assetSystem)
    : m_registry(registry), m_assetManager(assetSystem.assetManager())
{
    SPDLOG_INFO("Initializing AssetResolutionStage");
}

AssetResolutionStage::~AssetResolutionStage()
{
    SPDLOG_INFO("Destroying AssetResolutionStage");
}

void AssetResolutionStage::process(std::shared_ptr<RenderOrder> order)
{
    if (order->getType() != RenderOrderType::Mesh) {
        // Log warning for unexpected render order type
        SPDLOG_WARN("AssetResolutionStage: Received unexpected render order type '{}' for entity ID {}. This stage only processes Mesh orders.",
            renderOrderTypeToString(order->getType()), order->entity.id);

        // Forward to next stage and return
        forwardToNextStage(order);
        return;
    }

    try {
        // Get required components
        auto& meshComponent = m_registry.components().getComponent<MeshComponent>(order->entity);
        auto& materialComponent = m_registry.components().getComponent<MaterialComponent>(order->entity);

        // Get asset handles
        auto meshAssetHandle = meshComponent.getMesh();
        auto materialAssetHandle = materialComponent.getMaterial();

        SPDLOG_DEBUG("AssetResolutionStage: Ensuring assets are ready - Mesh: {}, Material: {}",
            meshAssetHandle.filename, materialAssetHandle.filename);

        // Check if both assets can be made ready
        bool meshReady = m_assetManager.ensureReady(meshAssetHandle);
        bool materialReady = m_assetManager.ensureReady(materialAssetHandle);

        if (!meshReady || !materialReady) {
            // One or both assets are not ready - discard the render order
            SPDLOG_WARN("AssetResolutionStage: Assets not ready for entity ID {} - Mesh ready: {}, Material ready: {}. Discarding render order.",
                order->entity.id, meshReady, materialReady);
            return; // Don't forward to next stage
        }

        auto meshOrder = std::static_pointer_cast<MeshRenderOrder>(order);

        // Resolve VramMesh - both assets are confirmed ready at this point
        meshOrder->meshHandle = m_assetManager.getHandle<MeshHandle>(meshAssetHandle);
        meshOrder->materialHandle = m_assetManager.getHandle<MaterialHandle>(materialAssetHandle);

        SPDLOG_DEBUG("AssetResolutionStage: Successfully resolved assets for entity ID {}", order->entity.id);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("AssetResolutionStage: Exception while processing mesh order for entity ID {}: {}. Discarding render order.",
            order->entity.id, e.what());
        return; // Don't forward to next stage on exception
    }

    // Forward to next stage only if everything succeeded
    forwardToNextStage(order);
}