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
        SPDLOG_DEBUG("AssetResolutionStage: Processing order of type {}, entity ID: {}",
            renderOrderTypeToString(order->getType()),
            order->entity.id);

        switch (order->getType()) {
        case RenderOrderType::Mesh:
            processMeshOrder(std::static_pointer_cast<MeshRenderOrder>(order));
            break;
        default:
            // Other order types don't need asset resolution
            SPDLOG_TRACE("AssetResolutionStage: Order type {} skipped (no assets to resolve)",
                renderOrderTypeToString(order->getType()));
            break;
        }

        // Forward to next stage
        forwardToNextStage(order);
        SPDLOG_TRACE("AssetResolutionStage: Order forwarded to next stage");
    }

private:
    void processMeshOrder(std::shared_ptr<MeshRenderOrder> order) {
        SPDLOG_DEBUG("AssetResolutionStage: Processing mesh order for entity ID {}",
            order->entity.id);

        try {
            // Get required components
            auto& meshComponent = m_registry.getComponent<MeshComponent>(order->entity);
            auto& materialComponent = m_registry.getComponent<MaterialComponent>(order->entity);

            // Log component handling
            SPDLOG_TRACE("AssetResolutionStage: Retrieved MeshComponent and MaterialComponent for entity ID {}",
                order->entity.id);

            // Ensure assets are loaded
            auto meshHandle = meshComponent.getMesh();
            auto materialHandle = materialComponent.getMaterial();

            SPDLOG_DEBUG("AssetResolutionStage: Ensuring assets are ready - Mesh: {}, Material: {}",
                meshHandle.filename, materialHandle.filename);

            m_assetManager.ensureReady(meshHandle);
            m_assetManager.ensureReady(materialHandle);

            // Resolve VramMesh
            const MeshHandle mesh = m_assetManager.getResource<MeshHandle>(meshHandle);
            if (mesh) {
                order->meshHandle = mesh;
                SPDLOG_DEBUG("AssetResolutionStage: Resolved mesh handle {} for entity ID {}",
                    mesh.id, order->entity.id);
            }
            else {
                SPDLOG_WARN("AssetResolutionStage: Failed to resolve mesh handle {} for entity ID {}",
                    meshHandle.filename, order->entity.id);
            }

            // Resolve Material
            order->materialHandle = m_assetManager.getResource<Material>(materialHandle);
            if (order->materialHandle) {
                SPDLOG_DEBUG("AssetResolutionStage: Resolved material handle {} for entity ID {}",
                    order->materialHandle.id, order->entity.id);
            }
            else {
                SPDLOG_WARN("AssetResolutionStage: Failed to resolve material handle {} for entity ID {}",
                    materialHandle.filename, order->entity.id);
            }
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("AssetResolutionStage: Exception while processing mesh order for entity ID {}: {}",
                order->entity.id, e.what());
        }
    }

    Registry& m_registry;
    AssetManager& m_assetManager;
};