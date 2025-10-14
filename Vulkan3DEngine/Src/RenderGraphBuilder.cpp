#include "RenderGraphBuilder.h"
#include "RenderGraph.h"
#include "RenderGraphTemplate.h"
#include "RenderNode.h"
#include "RenderNodeTemplate.h"
#include "RenderTarget.h"
#include "AttachmentManager.h"
#include "RenderPassManager.h"
#include "SwapChain.h"
#include <stdexcept>
#include <algorithm>

RenderGraphBuilder::RenderGraphBuilder(const RenderGraphTemplate& graphTemplate,
    const RenderTarget& target,
    AttachmentManager& attachmentMgr,
    RenderPassManager& renderPassMgr)
    : m_template(graphTemplate)
    , m_target(target)
    , m_extent{ 0, 0 }
    , m_attachmentManager(attachmentMgr)
    , m_renderPassManager(renderPassMgr)
    , m_requiresMSAAResolve(false)
    , m_targetFormat(VK_FORMAT_UNDEFINED)
    , m_targetSamples(VK_SAMPLE_COUNT_1_BIT) {

    // Validate preconditions
    if (!graphTemplate.isCompatibleWithTarget(target)) {
        throw std::invalid_argument("RenderGraphBuilder: template not compatible with target");
    }

    // Get dimensions from target
    m_extent = target.getDimensions();

    if (m_extent.width == 0 || m_extent.height == 0) {
        throw std::invalid_argument("RenderGraphBuilder: extent must be non-zero");
    }

    // Get format and samples from target
    m_targetSamples = static_cast<VkSampleCountFlagBits>(target.getSampleCount());

    if (target.isSwapchain()) {
        SwapChain* swapChain = target.getSwapChain();
        m_targetFormat = swapChain->getImageFormat();
        m_requiresMSAAResolve = (m_targetSamples != VK_SAMPLE_COUNT_1_BIT);
    }
    else {
        auto* textureInfo = target.getTextureHandle().get();
        m_targetFormat = Graphics::convertImageFormat(textureInfo->format);
        m_requiresMSAAResolve = (m_targetSamples != VK_SAMPLE_COUNT_1_BIT);
    }
}

std::unique_ptr<RenderGraph> RenderGraphBuilder::build() {
    // Build phases in order
    createNodes();
    createAttachments();
    createRenderPasses();

    // Create RenderGraph with built state - pass a COPY of the template
    auto graph = std::unique_ptr<RenderGraph>(
        new RenderGraph(
            m_template,  // Copy the template into the graph
            m_target,
            m_extent,
            std::move(m_nodes),
            std::move(m_attachments),
            std::move(m_ownedAttachments),
            m_requiresMSAAResolve,
            m_targetFormat,
            m_targetSamples,
            m_attachmentManager
        )
    );

    return graph;
}

void RenderGraphBuilder::createNodes() {
    auto nodeTemplatePtrs = m_template.getNodeTemplatePointers();

    m_nodes.reserve(nodeTemplatePtrs.size());
    m_attachments.resize(nodeTemplatePtrs.size());

    for (const auto* nodeTemplate : nodeTemplatePtrs) {
        if (!nodeTemplate) {
            throw std::runtime_error("RenderGraphBuilder: null node template in graph template");
        }

        const auto& spec = nodeTemplate->getAttachmentSpec();
        uint32_t attachmentCount = spec.getOutputCount();

        // For final node with swapchain + MSAA, add resolve attachment
        bool isFinalNode = (m_nodes.size() == nodeTemplatePtrs.size() - 1);
        if (isFinalNode && m_target.isSwapchain() && m_requiresMSAAResolve) {
            attachmentCount++;
        }

        m_attachments[m_nodes.size()].resize(attachmentCount);

        // Node creation completed after attachments and render passes are ready
        m_nodes.push_back(nullptr);
    }
}

void RenderGraphBuilder::createAttachments() {
    const auto& connections = m_template.getConnections();
    auto nodeTemplatePtrs = m_template.getNodeTemplatePointers();

    // Process connections to determine which attachments are needed
    for (const auto& connection : connections) {
        getOrCreateAttachment(connection.sourceNodeIndex, connection.sourceOutputIndex);
    }

    // Create output attachments for final nodes
    for (size_t nodeIdx = 0; nodeIdx < m_nodes.size(); ++nodeIdx) {
        const auto* nodeTemplate = nodeTemplatePtrs[nodeIdx];
        const auto& spec = nodeTemplate->getAttachmentSpec();
        bool isFinalNode = (nodeIdx == m_nodes.size() - 1);

        // Track if we need to add resolve attachment
        bool needsResolveAttachment = false;
        uint32_t colorAttachmentIndex = 0;

        for (uint32_t outIdx = 0; outIdx < spec.getOutputCount(); ++outIdx) {
            const auto& slot = spec.getOutputs()[outIdx];
            bool isConsumed = false;

            for (const auto& conn : connections) {
                if (conn.sourceNodeIndex == nodeIdx && conn.sourceOutputIndex == outIdx) {
                    isConsumed = true;
                    break;
                }
            }

            if (!isConsumed && isFinalNode) {
                GraphAttachment& attachment = m_attachments[nodeIdx][outIdx];

                // Determine attachment type
                bool isColorAttachment = (slot.role == AttachmentSlot::Role::Color);
                bool isDepthAttachment = (slot.role == AttachmentSlot::Role::Depth ||
                    slot.role == AttachmentSlot::Role::DepthStencil ||
                    slot.role == AttachmentSlot::Role::Stencil);

                if (m_target.isSwapchain()) {
                    if (isColorAttachment) {
                        if (m_requiresMSAAResolve) {
                            // MSAA color attachment needs internal storage + resolve
                            AttachmentImageSpec spec = m_attachmentManager.getFactory()
                                .createColorImageSpec(m_targetFormat, m_extent, m_targetSamples);

                            AttachmentHandle handle = m_attachmentManager.acquireAttachment(spec);
                            m_ownedAttachments.push_back(handle);

                            attachment.handle = handle;
                            attachment.format = m_targetFormat;
                            attachment.extent = m_extent;
                            attachment.samples = m_targetSamples;
                            attachment.isExternal = false;
                            attachment.isResolve = false;

                            needsResolveAttachment = true;
                            colorAttachmentIndex = outIdx;
                        }
                        else {
                            // Direct swapchain attachment (no MSAA)
                            attachment.isExternal = true;
                            attachment.format = m_targetFormat;
                            attachment.extent = m_extent;
                            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
                            attachment.isResolve = false;
                        }
                    }
                    else if (isDepthAttachment) {
                        // Depth attachments are NEVER external - always create internal
                        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

                        AttachmentImageSpec depthSpec;
                        if (slot.role == AttachmentSlot::Role::Depth) {
                            depthSpec = m_attachmentManager.getFactory()
                                .createDepthImageSpec(depthFormat, m_extent, m_targetSamples);
                        }
                        else {
                            depthSpec = m_attachmentManager.getFactory()
                                .createDepthStencilImageSpec(depthFormat, m_extent, m_targetSamples);
                        }

                        AttachmentHandle handle = m_attachmentManager.acquireAttachment(depthSpec);
                        m_ownedAttachments.push_back(handle);

                        attachment.handle = handle;
                        attachment.format = depthFormat;
                        attachment.extent = m_extent;
                        attachment.samples = m_targetSamples;
                        attachment.isExternal = false;
                        attachment.isResolve = false;
                    }
                }
                else if (m_target.isTexture()) {
                    if (m_requiresMSAAResolve && isColorAttachment) {
                        // MSAA color attachment needs internal storage + resolve
                        AttachmentImageSpec spec = m_attachmentManager.getFactory()
                            .createColorImageSpec(m_targetFormat, m_extent, m_targetSamples);

                        AttachmentHandle handle = m_attachmentManager.acquireAttachment(spec);
                        m_ownedAttachments.push_back(handle);

                        attachment.handle = handle;
                        attachment.format = m_targetFormat;
                        attachment.extent = m_extent;
                        attachment.samples = m_targetSamples;
                        attachment.isExternal = false;
                        attachment.isResolve = false;

                        needsResolveAttachment = true;
                        colorAttachmentIndex = outIdx;
                    }
                    else {
                        // Direct texture attachment
                        attachment.isExternal = true;
                        attachment.format = m_targetFormat;
                        attachment.extent = m_extent;
                        attachment.samples = isColorAttachment ? m_targetSamples : VK_SAMPLE_COUNT_1_BIT;
                        attachment.isResolve = false;
                    }
                }
            }
            else if (!isConsumed) {
                getOrCreateAttachment(static_cast<uint32_t>(nodeIdx), outIdx);
            }
        }

        // Add resolve attachment at the END of the attachment list
        if (needsResolveAttachment) {
            GraphAttachment resolveAttachment;
            resolveAttachment.isExternal = true;
            resolveAttachment.format = m_targetFormat;
            resolveAttachment.extent = m_extent;
            resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            resolveAttachment.isResolve = true;

            m_attachments[nodeIdx].back() = resolveAttachment;
        }
    }
}

void RenderGraphBuilder::createRenderPasses() {
    auto nodeTemplatePtrs = m_template.getNodeTemplatePointers();

    for (size_t nodeIdx = 0; nodeIdx < nodeTemplatePtrs.size(); ++nodeIdx) {
        const RenderNodeTemplate* nodeTemplate = nodeTemplatePtrs[nodeIdx];
        const auto& spec = nodeTemplate->getAttachmentSpec();
        bool isFinalNode = (nodeIdx == nodeTemplatePtrs.size() - 1);

        RenderPassConfig rpConfig;

        uint32_t colorAttachmentIndexInRP = UINT32_MAX;

        for (uint32_t outIdx = 0; outIdx < spec.getOutputCount(); ++outIdx) {
            const auto& slot = spec.getOutputs()[outIdx];
            const auto& attachment = m_attachments[nodeIdx][outIdx];

            VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageLayout finalLayout;

            // Determine final layout based on role and node position
            if (slot.role == AttachmentSlot::Role::Depth ||
                slot.role == AttachmentSlot::Role::DepthStencil) {
                finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
            else {
                finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                if (isFinalNode && !m_requiresMSAAResolve) {
                    if (m_target.isSwapchain()) {
                        finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                    }
                    else {
                        finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    }
                }
            }

            // Create appropriate image spec and attachment based on role
            RenderPassAttachment rpAttachment;

            if (slot.role == AttachmentSlot::Role::Color) {
                AttachmentImageSpec imageSpec = m_attachmentManager.getFactory()
                    .createColorImageSpec(attachment.format, attachment.extent, attachment.samples);

                rpAttachment = m_attachmentManager.getFactory()
                    .createColorAttachment(imageSpec, static_cast<uint32_t>(rpConfig.attachments.size()),
                        initialLayout, finalLayout);

                rpConfig.attachments.push_back(rpAttachment);

                colorAttachmentIndexInRP = static_cast<uint32_t>(rpConfig.attachments.size() - 1);
                rpConfig.colorAttachmentIndices.push_back(colorAttachmentIndexInRP);
            }
            else if (slot.role == AttachmentSlot::Role::Depth) {
                AttachmentImageSpec imageSpec = m_attachmentManager.getFactory()
                    .createDepthImageSpec(attachment.format, attachment.extent, attachment.samples);

                rpAttachment = m_attachmentManager.getFactory()
                    .createDepthAttachment(imageSpec, static_cast<uint32_t>(rpConfig.attachments.size()),
                        initialLayout, finalLayout);

                rpConfig.attachments.push_back(rpAttachment);
                rpConfig.depthAttachmentIndex = static_cast<uint32_t>(rpConfig.attachments.size() - 1);
            }
            else if (slot.role == AttachmentSlot::Role::DepthStencil) {
                AttachmentImageSpec imageSpec = m_attachmentManager.getFactory()
                    .createDepthStencilImageSpec(attachment.format, attachment.extent, attachment.samples);

                rpAttachment = m_attachmentManager.getFactory()
                    .createDepthStencilAttachment(imageSpec, static_cast<uint32_t>(rpConfig.attachments.size()),
                        initialLayout, finalLayout);

                rpConfig.attachments.push_back(rpAttachment);
                rpConfig.depthAttachmentIndex = static_cast<uint32_t>(rpConfig.attachments.size() - 1);
            }
            else if (slot.role == AttachmentSlot::Role::Stencil) {
                AttachmentImageSpec imageSpec = m_attachmentManager.getFactory()
                    .createDepthStencilImageSpec(attachment.format, attachment.extent, attachment.samples);

                rpAttachment = m_attachmentManager.getFactory()
                    .createDepthStencilAttachment(imageSpec, static_cast<uint32_t>(rpConfig.attachments.size()),
                        initialLayout, finalLayout);

                rpConfig.attachments.push_back(rpAttachment);
                rpConfig.depthAttachmentIndex = static_cast<uint32_t>(rpConfig.attachments.size() - 1);
            }
        }

        // Handle MSAA resolve attachment
        if (isFinalNode && m_requiresMSAAResolve) {
            const auto& resolveAttachment = m_attachments[nodeIdx].back();

            VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            if (m_target.isTexture()) {
                finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            AttachmentImageSpec resolveSpec = m_attachmentManager.getFactory()
                .createResolveImageSpec(resolveAttachment.format, resolveAttachment.extent);

            RenderPassAttachment rpResolve = m_attachmentManager.getFactory()
                .createResolveAttachment(resolveSpec, static_cast<uint32_t>(rpConfig.attachments.size()),
                    VK_IMAGE_LAYOUT_UNDEFINED, finalLayout);

            rpConfig.attachments.push_back(rpResolve);
            rpConfig.resolveAttachmentIndex = static_cast<uint32_t>(rpConfig.attachments.size() - 1);
        }

        SmartRenderPassHandle renderPass = m_renderPassManager.acquireSmartRenderPass(rpConfig);

        m_nodes[nodeIdx] = std::make_unique<RenderNode>(
            nodeTemplate,
            renderPass,
            m_extent
        );
    }
}

VkFormat RenderGraphBuilder::determineAttachmentFormat(uint32_t nodeIndex,
    uint32_t attachmentIndex,
    bool isOutput) const {

    bool isFinalNode = (nodeIndex == m_template.getNodeCount() - 1);

    if (isFinalNode && isOutput) {
        return m_targetFormat;
    }

    // Get the node template to check attachment role
    auto nodeTemplatePtrs = m_template.getNodeTemplatePointers();
    const auto* nodeTemplate = nodeTemplatePtrs[nodeIndex];
    const auto& spec = nodeTemplate->getAttachmentSpec();

    // Check if this is a depth attachment
    if (isOutput && attachmentIndex < spec.getOutputCount()) {
        const auto& slot = spec.getOutputs()[attachmentIndex];
        
        if (slot.role == AttachmentSlot::Role::Depth ||
            slot.role == AttachmentSlot::Role::DepthStencil ||
            slot.role == AttachmentSlot::Role::Stencil) {
            // Return a common depth format
            return VK_FORMAT_D32_SFLOAT;
        }
    }

    // Default to color format
    return VK_FORMAT_R8G8B8A8_SRGB;
}

GraphAttachment& RenderGraphBuilder::getOrCreateAttachment(uint32_t nodeIndex, uint32_t outputIndex) {
    GraphAttachment& attachment = m_attachments[nodeIndex][outputIndex];

    if (attachment.handle.isValid()) {
        return attachment;
    }

    VkFormat format = determineAttachmentFormat(nodeIndex, outputIndex, true);
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    // Get the node template to check attachment role
    auto nodeTemplatePtrs = m_template.getNodeTemplatePointers();
    const auto* nodeTemplate = nodeTemplatePtrs[nodeIndex];
    const auto& spec = nodeTemplate->getAttachmentSpec();
    const auto& slot = spec.getOutputs()[outputIndex];

    AttachmentImageSpec imageSpec;
    
    // Create appropriate image spec based on role
    if (slot.role == AttachmentSlot::Role::Color) {
        imageSpec = m_attachmentManager.getFactory()
            .createColorImageSpec(format, m_extent, samples);
    }
    else if (slot.role == AttachmentSlot::Role::Depth) {
        imageSpec = m_attachmentManager.getFactory()
            .createDepthImageSpec(format, m_extent, samples);
    }
    else if (slot.role == AttachmentSlot::Role::DepthStencil ||
             slot.role == AttachmentSlot::Role::Stencil) {
        imageSpec = m_attachmentManager.getFactory()
            .createDepthStencilImageSpec(format, m_extent, samples);
    }
    else {
        // Fallback to color
        imageSpec = m_attachmentManager.getFactory()
            .createColorImageSpec(format, m_extent, samples);
    }

    AttachmentHandle handle = m_attachmentManager.acquireAttachment(imageSpec);
    m_ownedAttachments.push_back(handle);

    attachment.handle = handle;
    attachment.format = format;
    attachment.extent = m_extent;
    attachment.samples = samples;
    attachment.isExternal = false;
    attachment.isResolve = false;

    return attachment;
}