#pragma once
#include "RenderTypes.h"
#include "Handle.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

// Forward declarations
class AttachmentManager;
class RenderPassManager;
class Renderer;
class AssetSystem;
class RenderOrder;

// Structure describing an attachment used by a render node
struct NodeAttachment {
    AttachmentHandle handle;
    uint32_t attachmentIndex;

    NodeAttachment(AttachmentHandle h, uint32_t idx)
        : handle(h), attachmentIndex(idx) {
    }
};

// Base class for statically cached render nodes
class RenderNode {
public:
    RenderNode(RenderTemplateType templateType,
        RenderPassHandle renderPassHandle,
        const RenderPassMetadata& metadata);
    virtual ~RenderNode() = default;

    // Non-copyable but movable
    RenderNode(const RenderNode&) = delete;
    RenderNode& operator=(const RenderNode&) = delete;
    RenderNode(RenderNode&&) = default;
    RenderNode& operator=(RenderNode&&) = default;

    // Basic getters
    RenderTemplateType getTemplateType() const { return m_templateType; }
    RenderPassHandle getRenderPassHandle() const { return m_renderPassHandle; }
    const RenderPassMetadata& getMetadata() const { return m_metadata; }
    VkExtent2D getRenderArea() const { return m_metadata.extent; }

    // Attachment access
    const std::vector<NodeAttachment>& getColorAttachments() const { return m_colorAttachments; }
    const NodeAttachment* getDepthAttachment() const { return m_depthAttachment; }
    const NodeAttachment* getResolveAttachment() const { return m_resolveAttachment; }

    // Get all attachments for framebuffer creation
    std::vector<AttachmentHandle> getAllAttachmentHandles() const;

    // Main execution method - orchestrates render pass execution
    void execute(VkCommandBuffer commandBuffer,
        FrameBufferHandle framebufferHandle,
        const std::vector<RenderOrder*>& renderOrders,
        Renderer& renderer,
        AssetSystem& assetSystem);

    // Validation
    bool isComplete() const;

protected:
    // Helper methods for derived classes
    void addColorAttachment(AttachmentHandle handle, uint32_t index);
    void addDepthAttachment(AttachmentHandle handle, uint32_t index);
    void addResolveAttachment(AttachmentHandle handle, uint32_t index);

    // Render pass management
    void beginRenderPass(VkCommandBuffer commandBuffer,
        VkRenderPass renderPass,
        VkFramebuffer framebuffer) const;
    void endRenderPass(VkCommandBuffer commandBuffer) const;

    // Virtual method for custom render node behavior (optional override)
    virtual void onBeforeRenderPass(VkCommandBuffer commandBuffer, Renderer& renderer) {}
    virtual void onAfterRenderPass(VkCommandBuffer commandBuffer, Renderer& renderer) {}

private:
    RenderTemplateType m_templateType;
    RenderPassHandle m_renderPassHandle;
    RenderPassMetadata m_metadata;

    // Attachment storage
    std::vector<NodeAttachment> m_colorAttachments;
    NodeAttachment* m_depthAttachment = nullptr;
    NodeAttachment* m_resolveAttachment = nullptr;
};