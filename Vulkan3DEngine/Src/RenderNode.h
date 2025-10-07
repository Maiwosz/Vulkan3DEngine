#pragma once
#include "RenderTypes.h"
#include "RenderTarget.h"
#include "Handle.h"
#include "CommandBuffer.h"
#include "ISmartHandleManager.h"
#include <vector>
#include <memory>
#include <map>

// Forward declarations
class RenderNodeTemplate;

// Smart handle type for render pass
using SmartRenderPassHandle = SmartHandle<RenderPassHandle, VkRenderPass>;

/**
 * Runtime render node instance with Vulkan resources.
 *
 * Architecture Role:
 * - Holds actual Vulkan resources (render pass, attachments)
 * - Created by templates, cached and reused by RenderNodeManager
 * - Provides render pass management and attachment binding
 * - Template reference allows accessing static metadata
 *
 * Nodes are lightweight - heavy lifting done during creation by template
 */

enum class AttachmentSource {
    Managed,      // Zwykły attachment z AttachmentManager
    Swapchain     // Attachment pochodzi ze SwapChain
};

 // Attachment binding information
struct NodeAttachment {
    AttachmentHandle handle;           // Dla Managed
    uint32_t attachmentIndex;          // Pozycja w render pass
    AttachmentSource source;           // Skąd pochodzi attachment

    // Konstruktor dla managed attachments
    NodeAttachment(AttachmentHandle h, uint32_t idx)
        : handle(h), attachmentIndex(idx), source(AttachmentSource::Managed) {
    }

    // Konstruktor dla swapchain attachments
    static NodeAttachment createSwapchainAttachment(uint32_t idx) {
        NodeAttachment attachment;
        attachment.handle = AttachmentHandle(0); // Nieważny, nie używany
        attachment.attachmentIndex = idx;
        attachment.source = AttachmentSource::Swapchain;
        return attachment;
    }

private:
    NodeAttachment() = default;
};

// Configured render node with Vulkan resources
class RenderNode {
public:
    RenderNode(const RenderNodeTemplate* nodeTemplate,
        const RenderTarget& renderTarget,
        VkExtent2D extent,
        SmartRenderPassHandle smartRenderPassHandle);

    virtual ~RenderNode() = default;

    // Non-copyable but movable
    RenderNode(const RenderNode&) = delete;
    RenderNode& operator=(const RenderNode&) = delete;
    RenderNode(RenderNode&&) = default;
    RenderNode& operator=(RenderNode&&) = default;

    // Basic properties
    const RenderNodeTemplate* getTemplate() const { return m_template; }
    const RenderTarget& getRenderTarget() const { return m_renderTarget; }
    VkExtent2D getExtent() const { return m_extent; }

    // Smart handle access
    const SmartRenderPassHandle& getSmartRenderPassHandle() const { return m_smartRenderPassHandle; }

    // Legacy compatibility - returns raw handle
    RenderPassHandle getRenderPassHandle() const { return m_smartRenderPassHandle.handle(); }

    // Direct access to VkRenderPass through smart handle
    VkRenderPass getRenderPass() const { return m_smartRenderPassHandle.isValid() ? *m_smartRenderPassHandle : VK_NULL_HANDLE; }

    // Template convenience methods
    std::type_index getTemplateTypeIndex() const;
    const char* getTemplateName() const;

    // Attachment access
    const std::vector<NodeAttachment>& getColorAttachments() const { return m_colorAttachments; }
    const NodeAttachment* getDepthAttachment() const { return m_depthAttachment.get(); }
    const NodeAttachment* getResolveAttachment() const { return m_resolveAttachment.get(); }

    // Sprawdź czy node używa swapchain
    bool usesSwapchainAttachment() const {
        return m_usesSwapchainColor || m_usesSwapchainResolve;
    }

    bool hasSwapchainColorAttachment() const { return m_usesSwapchainColor; }
    bool hasSwapchainResolveAttachment() const { return m_usesSwapchainResolve; }

    // Framebuffer creation helper
    std::vector<AttachmentHandle> getAllAttachmentHandles() const;

    // Validation
    bool isComplete() const;

    // Render pass control
    virtual void beginRenderPass(VkCommandBuffer commandBuffer,
        VkFramebuffer framebuffer,
        const VkExtent2D& renderArea) const;
    virtual void endRenderPass(VkCommandBuffer commandBuffer) const;

protected:
    // Setup interface - used by templates during node creation
    void addColorAttachment(AttachmentHandle handle, uint32_t index);
    void addDepthAttachment(AttachmentHandle handle, uint32_t index);
    void addResolveAttachment(AttachmentHandle handle, uint32_t index);

    // swapchain attachments
    void addSwapchainColorAttachment(uint32_t index);
    void addSwapchainResolveAttachment(uint32_t index);

    // Extension points for derived classes
    virtual void onBeforeRenderPass(CommandBuffer& commandBuffer) {}
    virtual void onAfterRenderPass(CommandBuffer& commandBuffer) {}

private:
    const RenderNodeTemplate* m_template;       // Static template reference
    RenderTarget m_renderTarget;                // Target specification
    VkExtent2D m_extent;                       // Cached dimensions
    SmartRenderPassHandle m_smartRenderPassHandle; // Smart handle to render pass

    // Attachment configuration
    std::vector<NodeAttachment> m_colorAttachments;
    std::unique_ptr<NodeAttachment> m_depthAttachment;    // Single depth attachment
    std::unique_ptr<NodeAttachment> m_resolveAttachment;  // Single resolve target

    bool m_usesSwapchainColor = false;
    bool m_usesSwapchainResolve = false;

    // Helper method to create properly ordered clear values
    std::vector<VkClearValue> createClearValues() const;

    friend class RenderNodeTemplate;  // Allow template to call protected setup methods
    friend class ForwardRenderNodeTemplate;
    friend class ImGuiRenderNodeTemplate;
};