#include "RenderNode.h"
#include "RenderOrder.h"
#include "Renderer.h"
#include "RenderPassManager.h"
#include "FrameBufferManager.h"
#include <stdexcept>

RenderNode::RenderNode(RenderTemplateType templateType,
    RenderPassHandle renderPassHandle,
    const RenderPassMetadata& metadata)
    : m_templateType(templateType)
    , m_renderPassHandle(renderPassHandle)
    , m_metadata(metadata) {
}

void RenderNode::execute(VkCommandBuffer commandBuffer,
    FrameBufferHandle framebufferHandle,
    const std::vector<RenderOrder*>& renderOrders,
    Renderer& renderer,
    AssetSystem& assetSystem) {
    // Get render pass and framebuffer from managers
    VkRenderPass* renderPass = renderer.renderPassManager().getResource(m_renderPassHandle);
    if (!renderPass) {
        throw std::runtime_error("Invalid render pass handle");
    }

    FrameBufferResource* framebufferResource = renderer.framebufferManager().getResource(framebufferHandle);
    if (!framebufferResource) {
        throw std::runtime_error("Invalid framebuffer handle");
    }

    // Pre-render pass hook
    onBeforeRenderPass(commandBuffer, renderer);

    // Begin render pass
    beginRenderPass(commandBuffer, *renderPass, framebufferResource->frameBuffer);

    // Execute all render orders polymorphically
    for (RenderOrder* renderOrder : renderOrders) {
        if (renderOrder) {
            renderOrder->execute(commandBuffer, renderer, assetSystem);
        }
    }

    // End render pass
    endRenderPass(commandBuffer);

    // Post-render pass hook
    onAfterRenderPass(commandBuffer, renderer);
}

void RenderNode::addColorAttachment(AttachmentHandle handle, uint32_t index) {
    m_colorAttachments.emplace_back(handle, index);
}

void RenderNode::addDepthAttachment(AttachmentHandle handle, uint32_t index) {
    if (m_depthAttachment) {
        throw std::runtime_error("RenderNode can only have one depth attachment");
    }
    m_depthAttachment = new NodeAttachment(handle, index);
}

void RenderNode::addResolveAttachment(AttachmentHandle handle, uint32_t index) {
    if (m_resolveAttachment) {
        throw std::runtime_error("RenderNode can only have one resolve attachment");
    }
    m_resolveAttachment = new NodeAttachment(handle, index);
}

std::vector<AttachmentHandle> RenderNode::getAllAttachmentHandles() const {
    std::vector<AttachmentHandle> handles;

    // Add color attachments
    for (const auto& attachment : m_colorAttachments) {
        handles.push_back(attachment.handle);
    }

    // Add depth attachment
    if (m_depthAttachment) {
        handles.push_back(m_depthAttachment->handle);
    }

    // Add resolve attachment
    if (m_resolveAttachment) {
        handles.push_back(m_resolveAttachment->handle);
    }

    return handles;
}

bool RenderNode::isComplete() const {
    // At minimum, we need at least one color attachment
    if (m_colorAttachments.empty()) {
        return false;
    }

    // If metadata indicates depth is needed, we must have depth attachment
    if (m_metadata.hasDepth && !m_depthAttachment) {
        return false;
    }

    // If metadata indicates resolve is needed, we must have resolve attachment
    if (m_metadata.hasResolve && !m_resolveAttachment) {
        return false;
    }

    return true;
}

void RenderNode::beginRenderPass(VkCommandBuffer commandBuffer,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer) const {
    // Setup clear values based on attachments
    std::vector<VkClearValue> clearValues;

    // Color attachments
    for (const auto& colorAttachment : m_colorAttachments) {
        VkClearValue clearValue = {};
        clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues.push_back(clearValue);
    }

    // Depth attachment
    if (m_depthAttachment) {
        VkClearValue clearValue = {};
        clearValue.depthStencil = { 1.0f, 0 };
        clearValues.push_back(clearValue);
    }

    // Resolve attachment (no clear value needed)
    if (m_resolveAttachment) {
        VkClearValue clearValue = {};
        clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues.push_back(clearValue);
    }

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass;
    beginInfo.framebuffer = framebuffer;
    beginInfo.renderArea.offset = { 0, 0 };
    beginInfo.renderArea.extent = m_metadata.extent;
    beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    beginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderNode::endRenderPass(VkCommandBuffer commandBuffer) const {
    vkCmdEndRenderPass(commandBuffer);
}