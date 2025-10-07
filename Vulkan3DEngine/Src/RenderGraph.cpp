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