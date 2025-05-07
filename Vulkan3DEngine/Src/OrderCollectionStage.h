#pragma once
#include "ProcessingStage.h"
#include <spdlog/spdlog.h>

class OrderCollectionStage : public OrderProcessingStage {
public:
    OrderCollectionStage() {
        SPDLOG_INFO("Initializing OrderCollectionStage");
    }

    ~OrderCollectionStage() {
        SPDLOG_INFO("Destroying OrderCollectionStage");
    }

    void process(std::shared_ptr<RenderOrder> order) override {
        // Log the order being processed
        SPDLOG_DEBUG("OrderCollectionStage: Processing order of type {}, entity ID: {}",
            renderOrderTypeToString(order->getType()),
            order->entity.id);

        // Simply forward the order to the next stage
        forwardToNextStage(order);

        SPDLOG_TRACE("OrderCollectionStage: Order forwarded to next stage");
    }
};