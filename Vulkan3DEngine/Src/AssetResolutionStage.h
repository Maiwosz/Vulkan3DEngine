#pragma once
#include "ProcessingStage.h"
#include "AssetManager.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"

class AssetResolutionStage : public OrderProcessingStage {
public:
    AssetResolutionStage(Registry& registry, AssetManager& assetManager)
        : m_registry(registry), m_assetManager(assetManager) {
    }

    void process(std::shared_ptr<RenderOrder> order) override {
        switch (order->getType()) {
        case RenderOrderType::Mesh:
            processMeshOrder(std::static_pointer_cast<MeshRenderOrder>(order));
            break;
        default:
            // Other order types don't need asset resolution
            break;
        }

        // Forward to next stage
        forwardToNextStage(order);
    }

private:
    void processMeshOrder(std::shared_ptr<MeshRenderOrder> order) {
        // Get required components
        auto& meshComponent = m_registry.getComponent<MeshComponent>(order->entity);
        auto& materialComponent = m_registry.getComponent<MaterialComponent>(order->entity);

        // Ensure assets are loaded
        auto meshHandle = meshComponent.getMesh();
        auto materialHandle = materialComponent.getMaterial();
        m_assetManager.ensureReady(meshHandle);
        m_assetManager.ensureReady(materialHandle);

        // Resolve VramMesh
        const auto* vramMesh = m_assetManager.getResource<VramMesh>(meshHandle);
        if (vramMesh) {
            order->vramMeshHandle = *vramMesh;
        }

        // Resolve Material
        order->materialHandle = m_assetManager.getResource<Material>(materialHandle);
    }

    Registry& m_registry;
    AssetManager& m_assetManager;
};