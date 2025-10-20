#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "Handle.h"
#include "RenderNode.h"

// Forward declarations
class RenderGraphTemplate;
class RenderTarget;
class AttachmentManager;
class RenderPassManager;
class SwapChain;
class RenderGraphBuilder;

struct GraphAttachment {
    AttachmentHandle handle;
    VkFormat format;
    VkExtent2D extent;
    VkSampleCountFlagBits samples;
    bool isExternal;
    bool isResolve;

    GraphAttachment()
        : handle(0)
        , format(VK_FORMAT_UNDEFINED)
        , extent{ 0, 0 }
        , samples(VK_SAMPLE_COUNT_1_BIT)
        , isExternal(false)
        , isResolve(false) {
    }
};

struct BoundAttachment {
    AttachmentHandle handle;
    uint32_t bindingIndex;

    BoundAttachment(AttachmentHandle h, uint32_t idx)
        : handle(h), bindingIndex(idx) {
    }
};

/**
 * Concrete render graph instance - describes structure and resources.
 *
 * Responsibilities:
 * - Provide access to graph structure (nodes, attachments, connections)
 * - Validate graph integrity
 * - Provide query interface for executor
 *
 * Construction delegated to RenderGraphBuilder.
 */
class RenderGraph {
public:
    // Use builder to construct
    friend class RenderGraphBuilder;

    ~RenderGraph();

    // Non-copyable but movable
    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) = default;
    RenderGraph& operator=(RenderGraph&&) = default;

    // ========== Structure Accessors ==========

    const RenderGraphTemplate& getTemplate() const { return m_template; }
    const RenderTarget& getRenderTarget() const { return m_target; }
    VkExtent2D getExtent() const { return m_extent; }

    const std::vector<std::unique_ptr<RenderNode>>& getNodes() const {
        return m_nodes;
    }

    RenderNode* getNode(size_t index);
    const RenderNode* getNode(size_t index) const;
    size_t getNodeCount() const { return m_nodes.size(); }

    // ========== Attachment Queries ==========

    const std::vector<GraphAttachment>& getNodeAttachments(uint32_t nodeIndex) const;

    std::vector<BoundAttachment> resolveNodeAttachments(
        uint32_t nodeIndex,
        uint32_t swapchainImageIndex = 0) const;

    std::vector<VkClearValue> getNodeClearValues(uint32_t nodeIndex) const;

    // ========== Validation ==========

    bool isValid() const;
    bool requiresMSAAResolve() const { return m_requiresMSAAResolve; }
    VkFormat getTargetFormat() const { return m_targetFormat; }
    VkSampleCountFlagBits getTargetSampleCount() const { return m_targetSamples; }
private:
    // Private constructor - only RenderGraphBuilder can create instances
    RenderGraph(const RenderGraphTemplate& graphTemplate,
        const RenderTarget& target,
        VkExtent2D extent,
        std::vector<std::unique_ptr<RenderNode>> nodes,
        std::vector<std::vector<GraphAttachment>> attachments,
        std::vector<AttachmentHandle> ownedAttachments,
        bool requiresMSAAResolve,
        VkFormat targetFormat,
        VkSampleCountFlagBits targetSamples,
        AttachmentManager& attachmentMgr);

    const RenderGraphTemplate& m_template;
    const RenderTarget& m_target;
    VkExtent2D m_extent;

    AttachmentManager& m_attachmentManager;

    // Built structure
    std::vector<std::unique_ptr<RenderNode>> m_nodes;
    std::vector<std::vector<GraphAttachment>> m_attachments;
    std::vector<AttachmentHandle> m_ownedAttachments;

    // Target properties
    bool m_requiresMSAAResolve;
    VkFormat m_targetFormat;
    VkSampleCountFlagBits m_targetSamples;
};