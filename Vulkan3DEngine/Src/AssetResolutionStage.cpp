#include "AssetResolutionStage.h"
#include "MeshRenderOrder.h"
#include "Mesh.h"

AssetResolutionStage::AssetResolutionStage(ProcessingContext& context, Registry& registry, AssetSystem& assetSystem)
	: ProcessingStage(context), m_registry(registry), m_assetManager(assetSystem.assetManager()), m_meshManager(assetSystem.meshManager())
{
    SPDLOG_INFO("Initializing AssetResolutionStage");
}

AssetResolutionStage::~AssetResolutionStage()
{
    SPDLOG_INFO("Destroying AssetResolutionStage");
}

ProcessingResult AssetResolutionStage::process(std::shared_ptr<RenderOrder> order)
{
    if (order->getType() != RenderOrderType::Mesh) {
        // Log warning for unexpected render order type
        SPDLOG_WARN("AssetResolutionStage: Received unexpected render order type '{}' for entity ID {}. This stage only processes Mesh orders.",
            renderOrderTypeToString(order->getType()), order->entity.id);
        return ProcessingResult::Failure; // Discard order
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
            return ProcessingResult::Failure; // Discard order
        }

        auto meshOrder = std::static_pointer_cast<MeshRenderOrder>(order);

        // Resolve asset handles - both assets are confirmed ready at this point
        meshOrder->meshHandle = m_assetManager.getHandle<MeshHandle>(meshAssetHandle);
        meshOrder->materialHandle = m_assetManager.getHandle<MaterialHandle>(materialAssetHandle);

        // Get mesh data and set it in DrawCall
        if (!setupDrawCallMeshData(*meshOrder)) {
            SPDLOG_ERROR("AssetResolutionStage: Failed to setup DrawCall mesh data for entity ID {}", order->entity.id);
            return ProcessingResult::Failure;
        }

        SPDLOG_DEBUG("AssetResolutionStage: Successfully resolved assets and setup DrawCall for entity ID {}", order->entity.id);
        return ProcessingResult::Success; // Order processed successfully
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("AssetResolutionStage: Exception while processing mesh order for entity ID {}: {}. Discarding render order.",
            order->entity.id, e.what());
        return ProcessingResult::Failure; // Discard order on exception
    }
}

bool AssetResolutionStage::setupDrawCallMeshData(MeshRenderOrder& meshOrder) {
    // Get mesh from asset system using the resolved handle
    const Mesh* mesh = m_meshManager.getMesh(meshOrder.meshHandle);
    if (!mesh) {
        SPDLOG_ERROR("AssetResolutionStage: Failed to get mesh asset for entity: {}", meshOrder.entity.id);
        return false;
    }

    // Create mesh data structure for DrawCall
    DrawCall::MeshData meshData;
    meshData.vertexBuffer = mesh->vertexBuffer;
    meshData.indexBuffer = mesh->indexBuffer;
    meshData.vertexCount = mesh->vertexCount;
    meshData.indexCount = mesh->indexCount;
    meshData.indexType = mesh->indexType;

    // Set mesh data in the DrawCall
    if (!meshOrder.drawCall) {
        SPDLOG_ERROR("AssetResolutionStage: DrawCall is null for entity: {}", meshOrder.entity.id);
        return false;
    }

    meshOrder.drawCall->setMeshData(meshData);

    SPDLOG_DEBUG("AssetResolutionStage: Setup DrawCall mesh data for entity: {} (vertices: {}, indices: {})",
        meshOrder.entity.id, meshData.vertexCount, meshData.indexCount);

    return true;
}