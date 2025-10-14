#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "Handle.h"
#include "RenderNode.h"

// Forward declarations
class RenderGraphTemplate;
class RenderNodeTemplate;
class RenderTarget;
class AttachmentManager;
class RenderPassManager;
class SwapChain;
class RenderGraph;

struct GraphAttachment;

/**
 * Builds concrete RenderGraph instances from templates.
 *
 * Responsibilities:
 * - Create RenderNode instances from templates
 * - Allocate intermediate attachments based on connections
 * - Create render passes with determined formats
 * - Manage MSAA resolve attachment creation
 *
 * This separates construction concerns from the RenderGraph's
 * responsibility of providing query access to the built structure.
 */
class RenderGraphBuilder {
public:
    RenderGraphBuilder(const RenderGraphTemplate& graphTemplate,
        const RenderTarget& target,
        AttachmentManager& attachmentMgr,
        RenderPassManager& renderPassMgr);

    /**
     * Build a concrete RenderGraph instance.
     * @return Fully constructed and validated RenderGraph
     * @throws std::runtime_error if build fails
     */
    std::unique_ptr<RenderGraph> build();

private:
    // Build phases
    void createNodes();
    void createAttachments();
    void createRenderPasses();

    // Attachment management
    VkFormat determineAttachmentFormat(uint32_t nodeIndex,
        uint32_t attachmentIndex,
        bool isOutput) const;

    GraphAttachment& getOrCreateAttachment(uint32_t nodeIndex, uint32_t outputIndex);

    // State needed for building
    const RenderGraphTemplate& m_template;
    const RenderTarget& m_target;
    VkExtent2D m_extent;

    AttachmentManager& m_attachmentManager;
    RenderPassManager& m_renderPassManager;

    // Build state (populated during construction phases)
    std::vector<std::unique_ptr<RenderNode>> m_nodes;
    std::vector<std::vector<GraphAttachment>> m_attachments;
    std::vector<AttachmentHandle> m_ownedAttachments;

    // Target properties
    bool m_requiresMSAAResolve;
    VkFormat m_targetFormat;
    VkSampleCountFlagBits m_targetSamples;
};