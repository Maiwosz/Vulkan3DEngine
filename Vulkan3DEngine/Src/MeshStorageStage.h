#pragma once
#include "ProcessingStage.h"
#include "MeshRenderOrder.h"
#include <spdlog/spdlog.h>

// Stage that stores fully processed mesh render orders in ProcessingContext
// for later culling by camera processing stages
class MeshStorageStage : public ProcessingStage {
public:
    MeshStorageStage(ProcessingContext& context) : ProcessingStage(context) {}

    ProcessingResult process(std::shared_ptr<RenderOrder> order) override {
        if (order->getType() != RenderOrderType::Mesh) {
            SPDLOG_WARN("MeshStorageStage received non-mesh order: {}",
                renderOrderTypeToString(order->getType()));
            return ProcessingResult::Failure;
        }

        auto meshOrder = std::static_pointer_cast<MeshRenderOrder>(order);

        // Verify mesh is fully processed and ready for rendering
        /*if (!meshOrder->isReadyForRendering()) {
            SPDLOG_WARN("Mesh order not ready for rendering, entity: {}", meshOrder->entity.id);
            return false;
        }*/

        m_context.addProcessedMesh(meshOrder);
        return ProcessingResult::Success;
    }
};