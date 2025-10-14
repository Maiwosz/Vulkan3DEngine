#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <memory>
#include <string>
#include "Handle.h"
#include "ISmartHandleManager.h"
#include "RenderGraph.h"
#include "RenderTarget.h"
#include "RenderGraphTemplateManager.h"

// Forward declarations
class RenderGraphTemplate;
class RenderTarget;
class AttachmentManager;
class RenderPassManager;

// Type aliases
using SmartRenderGraphHandle = SmartHandle<RenderGraphHandle, RenderGraph>;
using SmartRenderGraphTemplateHandle = SmartAssetHandle<RenderGraphTemplateHandle, RenderGraphTemplate>;

/**
 * Cache key for render graph instances.
 * Render graphs are cached based on template handle and target combination.
 */
struct RenderGraphCacheKey {
    RenderGraphTemplateHandle templateHandle;
    RenderTarget target;

    RenderGraphCacheKey(RenderGraphTemplateHandle tmpl, const RenderTarget& tgt)
        : templateHandle(tmpl), target(tgt) {
    }

    bool operator==(const RenderGraphCacheKey& other) const {
        return templateHandle == other.templateHandle && target == other.target;
    }

    size_t hash() const {
        size_t h1 = std::hash<uint32_t>{}(templateHandle.id);
        size_t h2 = target.hash();
        return h1 ^ (h2 << 1);
    }
};

namespace std {
    template<>
    struct hash<RenderGraphCacheKey> {
        size_t operator()(const RenderGraphCacheKey& key) const {
            return key.hash();
        }
    };
}

/**
 * Manages render graph instances with caching and smart handle support.
 *
 * Responsibilities:
 * - Cache render graphs by template and target
 * - Provide smart handles for automatic lifetime management
 * - Simplify render graph creation through centralized access to managers
 */
class RenderGraphManager : public ISmartHandleManager<RenderGraphHandle, RenderGraph> {
public:
    RenderGraphManager(
        AttachmentManager& attachmentMgr,
        RenderPassManager& renderPassMgr);

    ~RenderGraphManager();

    // Delete copy constructors
    RenderGraphManager(const RenderGraphManager&) = delete;
    RenderGraphManager& operator=(const RenderGraphManager&) = delete;

    /**
     * Get or create a render graph for the given template and target.
     * Returns a raw handle.
     */
    RenderGraphHandle acquireRenderGraph(
        const SmartRenderGraphTemplateHandle& graphTemplate,
        const RenderTarget& target);

    /**
     * Get or create a render graph for the given template and target.
     * Returns a smart handle with automatic reference counting.
     */
    SmartRenderGraphHandle acquireSmartRenderGraph(
        const SmartRenderGraphTemplateHandle& graphTemplate,
        const RenderTarget& target);

    /**
     * Invalidate cached render graphs for a specific target.
     * Useful when target properties change (e.g., window resize).
     */
    void invalidateTarget(const RenderTarget& target);

    /**
     * Invalidate cached render graphs for a specific template.
     * Useful when template is modified or reloaded.
     */
    void invalidateTemplate(RenderGraphTemplateHandle templateHandle);

    /**
     * Invalidate all cached render graphs.
     * Useful for major state changes or cleanup.
     */
    void invalidateAll();

    // IResourceManager interface
    RenderGraph* getResource(RenderGraphHandle handle) override;
    bool isValid(RenderGraphHandle handle) const override;
    void releaseResource(RenderGraphHandle handle) override;
    void addReference(RenderGraphHandle handle) override;
    void removeReference(RenderGraphHandle handle) override;

private:
    struct RenderGraphEntry {
        std::unique_ptr<RenderGraph> graph;
        RenderGraphCacheKey cacheKey;
        SmartRenderGraphTemplateHandle templateHandle;
        uint32_t referenceCount = 0;

        RenderGraphEntry() = delete;

        RenderGraphEntry(
            std::unique_ptr<RenderGraph> g,
            RenderGraphCacheKey key,
            SmartRenderGraphTemplateHandle tmplHandle,
            uint32_t refCount = 1)
            : graph(std::move(g))
            , cacheKey(key)
            , templateHandle(std::move(tmplHandle))
            , referenceCount(refCount) {
        }

        // Allow moving
        RenderGraphEntry(RenderGraphEntry&&) = default;
        RenderGraphEntry& operator=(RenderGraphEntry&&) = default;

        // Prevent copying
        RenderGraphEntry(const RenderGraphEntry&) = delete;
        RenderGraphEntry& operator=(const RenderGraphEntry&) = delete;
    };

    // Create a new render graph instance
    RenderGraphHandle createRenderGraph(
        const SmartRenderGraphTemplateHandle& graphTemplate,
        const RenderTarget& target);

    // Remove entry from cache maps
    void removeFromCache(RenderGraphHandle handle);

    AttachmentManager& m_attachmentManager;
    RenderPassManager& m_renderPassManager;

    // Cache: key -> handle mapping
    std::unordered_map<RenderGraphCacheKey, RenderGraphHandle> m_cache;

    // Storage: handle -> graph entry
    std::unordered_map<RenderGraphHandle, RenderGraphEntry> m_renderGraphs;

    uint32_t m_nextHandleId = 1;
};