#pragma once
#include "ProcessingStage.h"

class OrderCollectionStage : public OrderProcessingStage {
public:
    void process(std::shared_ptr<RenderOrder> order) override {
        // Simply forward the order to the next stage
        forwardToNextStage(order);
    }
};