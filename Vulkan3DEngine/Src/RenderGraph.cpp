#include "RenderGraph.h"
#include "RenderOrder.h"
#include <stdexcept>

RenderGraph::RenderGraph(std::type_index templateTypeIndex,
    const RenderTarget& renderTarget,
    VkExtent2D extent)
    : m_templateTypeIndex(templateTypeIndex)
    , m_renderTarget(renderTarget)
    , m_extent(extent) {
}

RenderGraphNode* RenderGraph::getNode(size_t index) {
    if (index >= m_nodes.size()) {
        return nullptr;
    }
    return &m_nodes[index];
}

const RenderGraphNode* RenderGraph::getNode(size_t index) const {
    if (index >= m_nodes.size()) {
        return nullptr;
    }
    return &m_nodes[index];
}

void RenderGraph::addNode(SmartHandle<RenderNodeHandle, RenderNode> nodeHandle) {
    if (!nodeHandle.isValid()) {
        throw std::invalid_argument("Cannot add invalid render node handle to graph");
    }

    m_nodes.emplace_back(std::move(nodeHandle));
}

void RenderGraph::addOrderToNode(size_t nodeIndex, RenderOrder* order) {
    if (nodeIndex >= m_nodes.size()) {
        throw std::out_of_range("Node index out of range");
    }

    if (!order) {
        throw std::invalid_argument("Cannot add null render order to node");
    }

    m_nodes[nodeIndex].renderOrders.push_back(order);
}

void RenderGraph::clearOrders() {
    for (auto& node : m_nodes) {
        node.renderOrders.clear();
    }
}