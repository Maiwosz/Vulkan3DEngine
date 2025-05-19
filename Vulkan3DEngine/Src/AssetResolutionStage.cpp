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
        // Forward to next stage and return
        forwardToNextStage(order);
        return;
    }

    try {
        // Get required components
        auto& meshComponent = m_registry.getComponent<MeshComponent>(order->entity);
        auto& materialComponent = m_registry.getComponent<MaterialComponent>(order->entity);

        // Ensure assets are loaded
        auto meshAssetHandle = meshComponent.getMesh();
        auto materialAssetHandle = materialComponent.getMaterial();

        SPDLOG_DEBUG("AssetResolutionStage: Ensuring assets are ready - Mesh: {}, Material: {}",
            meshAssetHandle.filename, materialAssetHandle.filename);

        m_assetManager.ensureReady(meshAssetHandle);
        m_assetManager.ensureReady(materialAssetHandle);

        auto meshOrder = std::static_pointer_cast<MeshRenderOrder>(order);

        // Resolve VramMesh
        meshOrder->meshHandle = m_assetManager.getHandle<MeshHandle>(meshAssetHandle);

        // Resolve Material
        meshOrder->materialHandle = m_assetManager.getHandle<MaterialHandle>(materialAssetHandle);

    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("AssetResolutionStage: Exception while processing mesh order for entity ID {}: {}",
            order->entity.id, e.what());
    }

    // Forward to next stage
    forwardToNextStage(order);
}
