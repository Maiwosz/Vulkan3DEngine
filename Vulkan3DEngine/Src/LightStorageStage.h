#pragma once
#include "ProcessingStage.h"
#include "LightRenderOrder.h"

// Simple stage that stores light render orders in ProcessingContext
// for later use by camera processing stages
class LightStorageStage : public ProcessingStage {
public:
    LightStorageStage(ProcessingContext& context) : ProcessingStage(context) {}

    ProcessingResult process(std::shared_ptr<RenderOrder> order) override {
        if (order->getType() != RenderOrderType::Light) {
            SPDLOG_WARN("LightStorageStage received non-light order: {}",
                renderOrderTypeToString(order->getType()));
            return ProcessingResult::Failure;
        }

        auto lightOrder = std::static_pointer_cast<LightRenderOrder>(order);
        m_context.addLight(lightOrder);
        return ProcessingResult::Success;
    }
};