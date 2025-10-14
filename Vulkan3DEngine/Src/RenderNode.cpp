#include "RenderNode.h"
#include <stdexcept>
#include <algorithm>
#include <map>

RenderNode::RenderNode(const RenderNodeTemplate* nodeTemplate,
    SmartRenderPassHandle renderPass,
    VkExtent2D extent)
    : m_template(nodeTemplate)
    , m_renderPass(renderPass)
    , m_extent(extent) {

    if (!m_template) {
        throw std::invalid_argument("RenderNode: nodeTemplate cannot be null");
    }

    if (!m_renderPass.isValid()) {
        throw std::invalid_argument("RenderNode: renderPass must be valid");
    }
}

bool RenderNode::isValid() const {
    // Must have a template
    if (!m_template) {
        return false;
    }

    // Must have a valid render pass
    if (!m_renderPass.isValid()) {
        return false;
    }

    // Extent must be non-zero
    if (m_extent.width == 0 || m_extent.height == 0) {
        return false;
    }

    return true;
}