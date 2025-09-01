#include "RenderNodeManager.h"
#include "Renderer.h"
#include <stdexcept>

RenderNodeManager::RenderNodeManager(RenderPassManager& renderPassManager,
    AttachmentManager& attachmentManager)
    : m_renderPassManager(renderPassManager)
    , m_attachmentManager(attachmentManager) {
}

RenderNodeManager::~RenderNodeManager() {
    clearCache();
}

void RenderNodeManager::registerTemplate(std::unique_ptr<RenderNodeTemplate> nodeTemplate) {
    if (!nodeTemplate) {
        throw std::invalid_argument("Cannot register null render node template");
    }

    RenderTemplateType templateType = nodeTemplate->getTemplateType();
    size_t index = static_cast<size_t>(templateType);

    if (index >= m_templates.size()) {
        throw std::invalid_argument("Invalid template type");
    }

    if (m_templates[index]) {
        throw std::invalid_argument("Template already registered for this type");
    }

    m_templates[index] = std::move(nodeTemplate);
}

RenderNode* RenderNodeManager::getCachedNode(RenderTemplateType templateType,
    const RenderPassMetadata& metadata) {
    // Create cache key
    RenderNodeCacheKey key = { templateType, metadata };

    // Check cache first
    auto it = m_nodeCache.find(key);
    if (it != m_nodeCache.end()) {
        return it->second.get();
    }

    // Create new node
    auto node = createRenderNode(templateType, metadata);
    if (!node) {
        return nullptr;
    }

    RenderNode* nodePtr = node.get();
    m_nodeCache[key] = std::move(node);

    return nodePtr;
}

void RenderNodeManager::executeRenderPass(RenderTemplateType templateType,
    const RenderPassMetadata& metadata,
    VkCommandBuffer commandBuffer,
    FrameBufferHandle framebufferHandle,
    const std::vector<RenderOrder*>& renderOrders,
    Renderer& renderer,
    AssetSystem& assetSystem) {
    // Get cached node for this template type
    RenderNode* node = getCachedNode(templateType, metadata);
    if (!node) {
        throw std::runtime_error("Failed to get render node for template type");
    }

    // Execute the render pass
    node->execute(commandBuffer, framebufferHandle, renderOrders, renderer, assetSystem);
}

void RenderNodeManager::clearCache() {
    m_nodeCache.clear();
}

size_t RenderNodeManager::getCacheSize() const {
    return m_nodeCache.size();
}

bool RenderNodeManager::hasTemplate(RenderTemplateType templateType) const {
    size_t index = static_cast<size_t>(templateType);
    return index < m_templates.size() && m_templates[index] != nullptr;
}

RenderNodeTemplate* RenderNodeManager::getTemplate(RenderTemplateType templateType) const {
    size_t index = static_cast<size_t>(templateType);
    if (index >= m_templates.size()) {
        return nullptr;
    }
    return m_templates[index].get();
}

std::unique_ptr<RenderNode> RenderNodeManager::createRenderNode(RenderTemplateType templateType,
    const RenderPassMetadata& metadata) {
    // Get template
    RenderNodeTemplate* nodeTemplate = getTemplate(templateType);
    if (!nodeTemplate) {
        throw std::runtime_error("No template registered for template type");
    }

    // Check compatibility
    if (!nodeTemplate->isCompatible(metadata)) {
        throw std::runtime_error("Metadata not compatible with template");
    }

    // Create render node
    return nodeTemplate->createRenderNode(m_renderPassManager, m_attachmentManager, metadata);
}