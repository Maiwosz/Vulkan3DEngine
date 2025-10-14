#include "RenderGraph.h"
#include "RenderGraphTemplate.h"
#include "RenderNode.h"
#include "AttachmentManager.h"
#include "RenderPassManager.h"
#include "SwapChain.h"
#include "RenderTarget.h"
#include <stdexcept>
#include <algorithm>

RenderGraph::RenderGraph(const RenderGraphTemplate& graphTemplate,
    const RenderTarget& target,
    VkExtent2D extent,
    std::vector<std::unique_ptr<RenderNode>> nodes,
    std::vector<std::vector<GraphAttachment>> attachments,
    std::vector<AttachmentHandle> ownedAttachments,
    bool requiresMSAAResolve,
    VkFormat targetFormat,
    VkSampleCountFlagBits targetSamples,
    AttachmentManager& attachmentMgr)
    : m_template(graphTemplate)
    , m_target(target)
    , m_extent(extent)
    , m_attachmentManager(attachmentMgr)
    , m_nodes(std::move(nodes))
    , m_attachments(std::move(attachments))
    , m_ownedAttachments(std::move(ownedAttachments))
    , m_requiresMSAAResolve(requiresMSAAResolve)
    , m_targetFormat(targetFormat)
    , m_targetSamples(targetSamples) {
}

RenderGraph::~RenderGraph() {
    // Release owned attachments
    for (AttachmentHandle handle : m_ownedAttachments) {
        m_attachmentManager.releaseResource(handle);
    }
}

const std::vector<GraphAttachment>& RenderGraph::getNodeAttachments(uint32_t nodeIndex) const {
    if (nodeIndex >= m_attachments.size()) {
        static const std::vector<GraphAttachment> empty;
        return empty;
    }
    return m_attachments[nodeIndex];
}

std::vector<BoundAttachment> RenderGraph::resolveNodeAttachments(
    uint32_t nodeIndex,
    uint32_t swapchainImageIndex) const {

    std::vector<BoundAttachment> boundAttachments;

    if (nodeIndex >= m_attachments.size()) {
        return boundAttachments;
    }

    const auto& connections = m_template.getConnections();
    auto nodeTemplatePtrs = m_template.getNodeTemplatePointers();
    const auto* nodeTemplate = nodeTemplatePtrs[nodeIndex];
    const auto& spec = nodeTemplate->getAttachmentSpec();
    bool isFinalNode = (nodeIndex == m_template.getNodeCount() - 1);

    uint32_t attachmentIndex = 0;

    // Bind input attachments from connections
    for (uint32_t inIdx = 0; inIdx < spec.getInputCount(); ++inIdx) {
        for (const auto& conn : connections) {
            if (conn.targetNodeIndex == nodeIndex && conn.targetInputIndex == inIdx) {
                const auto& sourceAttachment = m_attachments[conn.sourceNodeIndex][conn.sourceOutputIndex];
                boundAttachments.emplace_back(sourceAttachment.handle, attachmentIndex++);
                break;
            }
        }
    }

    // Bind output attachments
    for (uint32_t outIdx = 0; outIdx < spec.getOutputCount(); ++outIdx) {
        const auto& attachment = m_attachments[nodeIndex][outIdx];

        if (attachment.isExternal && m_target.isSwapchain() && isFinalNode && !m_requiresMSAAResolve) {
            SwapChain* swapChain = m_target.getSwapChain();
            const auto& swapchainAttachments = swapChain->getAttachmentHandles();
            boundAttachments.emplace_back(swapchainAttachments[swapchainImageIndex], attachmentIndex++);
        }
        else {
            boundAttachments.emplace_back(attachment.handle, attachmentIndex++);
        }
    }

    // Bind resolve attachment if needed
    if (isFinalNode && m_requiresMSAAResolve) {
        const auto& resolveAttachment = m_attachments[nodeIndex].back();

        if (m_target.isSwapchain()) {
            SwapChain* swapChain = m_target.getSwapChain();
            const auto& swapchainAttachments = swapChain->getAttachmentHandles();
            boundAttachments.emplace_back(swapchainAttachments[swapchainImageIndex], attachmentIndex++);
        }
        else {
            boundAttachments.emplace_back(resolveAttachment.handle, attachmentIndex++);
        }
    }

    return boundAttachments;
}

std::vector<VkClearValue> RenderGraph::getNodeClearValues(uint32_t nodeIndex) const {
    std::vector<VkClearValue> clearValues;

    if (nodeIndex >= m_nodes.size()) {
        return clearValues;
    }

    auto nodeTemplatePtrs = m_template.getNodeTemplatePointers();
    const auto* nodeTemplate = nodeTemplatePtrs[nodeIndex];
    const auto& spec = nodeTemplate->getAttachmentSpec();
    bool isFinalNode = (nodeIndex == m_template.getNodeCount() - 1);

    // Clear values for outputs
    for (uint32_t outIdx = 0; outIdx < spec.getOutputCount(); ++outIdx) {
        const auto& slot = spec.getOutputs()[outIdx];
        VkClearValue clearValue{};

        if (slot.role == AttachmentSlot::Role::Depth ||
            slot.role == AttachmentSlot::Role::DepthStencil) {
            clearValue.depthStencil = { 1.0f, 0 };
        }
        else {
            clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        }

        clearValues.push_back(clearValue);
    }

    // Add clear value for resolve attachment if present
    if (isFinalNode && m_requiresMSAAResolve) {
        VkClearValue clearValue{};
        clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues.push_back(clearValue);
    }

    return clearValues;
}

RenderNode* RenderGraph::getNode(size_t index) {
    if (index >= m_nodes.size()) {
        return nullptr;
    }
    return m_nodes[index].get();
}

const RenderNode* RenderGraph::getNode(size_t index) const {
    if (index >= m_nodes.size()) {
        return nullptr;
    }
    return m_nodes[index].get();
}

bool RenderGraph::isValid() const {
    if (m_nodes.empty()) {
        return false;
    }

    for (const auto& node : m_nodes) {
        if (!node || !node->isValid()) {
            return false;
        }
    }

    return true;
}