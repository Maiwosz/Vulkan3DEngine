#pragma once
#include "RenderTypes.h"
#include "RenderNode.h"
#include "RenderNodeTemplate.h"
#include "RenderPassManager.h"
#include "AttachmentManager.h"
#include "RenderOrder.h"
#include <memory>
#include <unordered_map>
#include <array>
#include <vector>

// Forward declarations
class Renderer;
class AssetSystem;

class RenderNodeManager {
public:
    RenderNodeManager(RenderPassManager& renderPassManager,
        AttachmentManager& attachmentManager);
    ~RenderNodeManager();

    // Non-copyable
    RenderNodeManager(const RenderNodeManager&) = delete;
    RenderNodeManager& operator=(const RenderNodeManager&) = delete;

    // Template registration
    void registerTemplate(std::unique_ptr<RenderNodeTemplate> nodeTemplate);

    // Get cached render node for given template type and metadata
    RenderNode* getCachedNode(RenderTemplateType templateType,
        const RenderPassMetadata& metadata);

    // Main render method - executes render pass using cached node
    void executeRenderPass(RenderTemplateType templateType,
        const RenderPassMetadata& metadata,
        VkCommandBuffer commandBuffer,
        FrameBufferHandle framebufferHandle,
        const std::vector<RenderOrder*>& renderOrders,
        Renderer& renderer,
        AssetSystem& assetSystem);

    // Cache management
    void clearCache();
    size_t getCacheSize() const;

    // Template access
    bool hasTemplate(RenderTemplateType templateType) const;
    RenderNodeTemplate* getTemplate(RenderTemplateType templateType) const;

private:
    RenderPassManager& m_renderPassManager;
    AttachmentManager& m_attachmentManager;

    // Template registry (indexed by template type)
    std::array<std::unique_ptr<RenderNodeTemplate>,
        static_cast<size_t>(RenderTemplateType::Count)> m_templates;

    // Cache for render nodes
    std::unordered_map<RenderNodeCacheKey, std::unique_ptr<RenderNode>> m_nodeCache;

    // Helper methods
    std::unique_ptr<RenderNode> createRenderNode(RenderTemplateType templateType,
        const RenderPassMetadata& metadata);
};