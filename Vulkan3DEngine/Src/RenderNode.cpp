#include "RenderNode.h"
#include "RenderNodeTemplate.h"
#include <stdexcept>

RenderNode::RenderNode(const RenderNodeTemplate* nodeTemplate,
    const RenderTarget& renderTarget,
    VkExtent2D extent,
    SmartRenderPassHandle smartRenderPassHandle)
    : m_template(nodeTemplate)
    , m_renderTarget(renderTarget)
    , m_extent(extent)
    , m_smartRenderPassHandle(std::move(smartRenderPassHandle)) {
}

std::type_index RenderNode::getTemplateTypeIndex() const {
    return m_template ? m_template->getTypeIndex() : std::type_index(typeid(void));
}

const char* RenderNode::getTemplateName() const {
    return m_template ? m_template->getTemplateName() : "Unknown";
}

void RenderNode::addColorAttachment(AttachmentHandle handle, uint32_t index) {
    m_colorAttachments.emplace_back(handle, index);
}

void RenderNode::addDepthAttachment(AttachmentHandle handle, uint32_t index) {
    if (m_depthAttachment) {
        throw std::runtime_error("RenderNode can only have one depth attachment");
    }
    m_depthAttachment = std::make_unique<NodeAttachment>(handle, index);
}

void RenderNode::addResolveAttachment(AttachmentHandle handle, uint32_t index) {
    if (m_resolveAttachment) {
        throw std::runtime_error("RenderNode can only have one resolve attachment");
    }
    m_resolveAttachment = std::make_unique<NodeAttachment>(handle, index);
}

void RenderNode::addSwapchainColorAttachment(uint32_t index) {
    m_colorAttachments.push_back(NodeAttachment::createSwapchainAttachment(index));
    m_usesSwapchainColor = true;
}

void RenderNode::addSwapchainResolveAttachment(uint32_t index) {
    if (m_resolveAttachment) {
        throw std::runtime_error("RenderNode can only have one resolve attachment");
    }
    m_resolveAttachment = std::make_unique<NodeAttachment>(
        NodeAttachment::createSwapchainAttachment(index));
    m_usesSwapchainResolve = true;
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

    // Check if smart render pass handle is valid
    if (!m_smartRenderPassHandle.isValid()) {
        return false;
    }

    // Additional validation could be added based on template requirements
    // For now, basic validation is sufficient
    return true;
}

void RenderNode::beginRenderPass(VkCommandBuffer commandBuffer,
    VkFramebuffer framebuffer,
    const VkExtent2D& renderArea) const {

    // Verify we have a valid render pass
    if (!m_smartRenderPassHandle.isValid()) {
        throw std::runtime_error("RenderNode has invalid render pass handle");
    }

    VkRenderPass renderPass = getRenderPass();
    if (renderPass == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to get VkRenderPass from smart handle");
    }

    // Setup clear values based on actual attachment order and indices
    std::vector<VkClearValue> clearValues = createClearValues();

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass;
    beginInfo.framebuffer = framebuffer;
    beginInfo.renderArea.offset = { 0, 0 };
    beginInfo.renderArea.extent = renderArea;
    beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    beginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderNode::endRenderPass(VkCommandBuffer commandBuffer) const {
    vkCmdEndRenderPass(commandBuffer);
}

std::vector<VkClearValue> RenderNode::createClearValues() const {
    // Create a map of attachment index to clear value
    std::map<uint32_t, VkClearValue> indexToClearValue;

    // Add color attachments
    for (const auto& colorAttachment : m_colorAttachments) {
        VkClearValue clearValue = {};
        clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        indexToClearValue[colorAttachment.attachmentIndex] = clearValue;
    }

    // Add depth attachment
    if (m_depthAttachment) {
        VkClearValue clearValue = {};
        clearValue.depthStencil = { 1.0f, 0 };
        indexToClearValue[m_depthAttachment->attachmentIndex] = clearValue;
    }

    // Add resolve attachment
    if (m_resolveAttachment) {
        VkClearValue clearValue = {};
        clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        indexToClearValue[m_resolveAttachment->attachmentIndex] = clearValue;
    }

    // Convert map to vector, ensuring proper order by index
    std::vector<VkClearValue> clearValues;
    if (!indexToClearValue.empty()) {
        uint32_t maxIndex = indexToClearValue.rbegin()->first;
        clearValues.resize(maxIndex + 1);

        for (const auto& [index, clearValue] : indexToClearValue) {
            clearValues[index] = clearValue;
        }
    }

    return clearValues;
}