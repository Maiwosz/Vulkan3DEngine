#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include "RenderOrder.h"
#include "Registry.h"

class OrderProcessingStage {
public:
    virtual ~OrderProcessingStage() = default;

    // Process a single render order and optionally pass it to next stages
    virtual void process(std::shared_ptr<RenderOrder> order) = 0;

    // Process a batch of render orders
    virtual void processBatch(const std::vector<std::shared_ptr<RenderOrder>>& orders) {
        for (const auto& order : orders) {
            process(order);
        }
    }

    // Connect this stage to next stages based on render order type
    void connectTo(RenderOrderType type, std::shared_ptr<OrderProcessingStage> nextStage) {
        m_nextStages[type] = nextStage;
    }

protected:
    // Forward render order to the next appropriate stage
    void forwardToNextStage(std::shared_ptr<RenderOrder> order) {
        auto type = order->getType();
        auto it = m_nextStages.find(type);
        if (it != m_nextStages.end() && it->second) {
            it->second->process(order);
        }
    }

    std::unordered_map<RenderOrderType, std::shared_ptr<OrderProcessingStage>> m_nextStages;
};