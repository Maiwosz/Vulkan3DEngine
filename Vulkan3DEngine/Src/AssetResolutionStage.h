#pragma once
#include "ProcessingStage.h"
#include "AssetManager.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include <spdlog/spdlog.h>

class AssetResolutionStage : public OrderProcessingStage {
public:
    AssetResolutionStage(Registry& registry, AssetManager& assetManager)
        : m_registry(registry), m_assetManager(assetManager) {
        SPDLOG_INFO("Initializing AssetResolutionStage");
    }

    ~AssetResolutionStage() {
        SPDLOG_INFO("Destroying AssetResolutionStage");
    }

    void process(std::shared_ptr<RenderOrder> order) override {

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
            auto meshHandle = meshComponent.getMesh();
            auto materialHandle = materialComponent.getMaterial();

            SPDLOG_DEBUG("AssetResolutionStage: Ensuring assets are ready - Mesh: {}, Material: {}",
                meshHandle.filename, materialHandle.filename);

            m_assetManager.ensureReady(meshHandle);
            m_assetManager.ensureReady(materialHandle);

            auto meshOrder = std::static_pointer_cast<MeshRenderOrder>(order);

            // Resolve VramMesh
            meshOrder->meshHandle = m_assetManager.getResource<MeshHandle>(meshHandle);

            // Resolve Material
            meshOrder->materialHandle = m_assetManager.getResource<Material>(materialHandle);

        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("AssetResolutionStage: Exception while processing mesh order for entity ID {}: {}",
                order->entity.id, e.what());
        }

        // Forward to next stage
        forwardToNextStage(order);
    }

private:
    Registry& m_registry;
    AssetManager& m_assetManager;
};