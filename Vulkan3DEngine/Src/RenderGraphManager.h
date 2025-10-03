#pragma once
#include "RenderGraph.h"
#include "RenderGraphTemplate.h"
#include "ISmartHandleManager.h"
#include <memory>
#include <unordered_map>
#include <typeindex>

// Forward declarations
class EngineCore;
class RenderNodeManager;
class RenderPassManager;
class AttachmentManager;
class FrameBufferManager;
class TextureManager;
class SwapChain;

/**
 * Render graph instance cache and orchestrator.
 *
 * Architecture Role:
 * - Caches complete render graphs by (template type, target type, dimensions)
 * - Orchestrates RenderNodeManager to acquire individual nodes for graphs
 * - Manages template registry for graph creation
 * - Entry point for requesting complex multi-pass rendering setups
 *
 * Usage: Request graph by template + target → get cached or create new graph
 */
class RenderGraphManager : public ISmartHandleManager<RenderGraphHandle, RenderGraph> {
public:
    RenderGraphManager(EngineCore& engineCore);
    ~RenderGraphManager();

    // Non-copyable
    RenderGraphManager(const RenderGraphManager&) = delete;
    RenderGraphManager& operator=(const RenderGraphManager&) = delete;

    // ISmartHandleManager interface
    RenderGraph* getResource(RenderGraphHandle handle) override;
    bool isValid(RenderGraphHandle handle) const override;
    void releaseResource(RenderGraphHandle handle) override;
    void addReference(RenderGraphHandle handle) override;
    void removeReference(RenderGraphHandle handle) override;

    // Type-based template management - no runtime registration needed
    template<typename GraphTemplateType>
    bool hasTemplate() const {
        return m_templates.find(std::type_index(typeid(GraphTemplateType))) != m_templates.end();
    }

    template<typename GraphTemplateType>
    void registerTemplate(std::unique_ptr<GraphTemplateType> graphTemplate) {
        static_assert(std::is_base_of_v<RenderGraphTemplate, GraphTemplateType>,
            "GraphTemplateType must derive from RenderGraphTemplate");
        m_templates[std::type_index(typeid(GraphTemplateType))] = std::move(graphTemplate);
    }

    template<typename GraphTemplateType>
    GraphTemplateType* getTemplate() const {
        auto it = m_templates.find(std::type_index(typeid(GraphTemplateType)));
        if (it != m_templates.end()) {
            return static_cast<GraphTemplateType*>(it->second.get());
        }
        return nullptr;
    }

    // Graph acquisition with automatic template creation if not registered
    template<typename GraphTemplateType>
    SmartHandle<RenderGraphHandle, RenderGraph> acquireSmartGraph(const RenderTarget& renderTarget) {
        // Auto-register template if not present
        if (!hasTemplate<GraphTemplateType>()) {
            registerTemplate(std::make_unique<GraphTemplateType>(m_engineCore));
        }

        auto* graph = getGraph<GraphTemplateType>(renderTarget);
        if (!graph) {
            return SmartHandle<RenderGraphHandle, RenderGraph>(); // Invalid handle
        }

        // Find the handle for this graph
        for (const auto& [handle, resource] : m_resources) {
            if (resource.graph.get() == graph) {
                return createSmartHandle(handle);
            }
        }

        return SmartHandle<RenderGraphHandle, RenderGraph>(); // Should not happen
    }

    // Direct access (for cases not needing smart handles)
    template<typename GraphTemplateType>
    RenderGraph* getGraph(const RenderTarget& renderTarget) {
        // Validate render target
        if (!renderTarget.isValid()) {
            throw std::invalid_argument("Invalid render target");
        }

        // Extract dimensions directly from render target
        VkExtent2D extent = extractDimensions(renderTarget);

        // Create cache key
        RenderGraphCacheKey<GraphTemplateType> cacheKey;
        cacheKey.targetType = renderTarget.getType();
        cacheKey.extent = extent;

        // Look for existing cached graph
        auto cacheIt = m_graphCache.find(std::type_index(typeid(GraphTemplateType)));
        if (cacheIt != m_graphCache.end()) {
            auto& typeCache = cacheIt->second;
            auto keyHash = std::hash<RenderGraphCacheKey<GraphTemplateType>>{}(cacheKey);
            auto graphIt = typeCache.find(keyHash);
            if (graphIt != typeCache.end()) {
                auto resourceIt = m_resources.find(graphIt->second);
                if (resourceIt != m_resources.end() && !resourceIt->second.markedForDeletion) {
                    return resourceIt->second.graph.get();
                }
                else {
                    // Handle became invalid, remove from cache
                    typeCache.erase(graphIt);
                }
            }
        }

        // Create new graph if not found
        auto handle = createRenderGraph<GraphTemplateType>(renderTarget);
        if (!handle.isValid()) {
            return nullptr;
        }

        // Cache it
        auto keyHash = std::hash<RenderGraphCacheKey<GraphTemplateType>>{}(cacheKey);
        m_graphCache[std::type_index(typeid(GraphTemplateType))][keyHash] = handle;

        return getResource(handle);
    }

    // Cache management
    void clearCache();
    size_t getCacheSize() const;

    // Helper to extract dimensions from RenderTarget
    VkExtent2D extractDimensions(const RenderTarget& target) const;

private:
    struct GraphResource {
        std::unique_ptr<RenderGraph> graph;
        uint32_t referenceCount = 0;
        bool markedForDeletion = false;
    };

    // Dependencies
	EngineCore& m_engineCore;
    RenderNodeManager& m_nodeManager;
    RenderPassManager& m_renderPassManager;
    AttachmentManager& m_attachmentManager;
    FrameBufferManager& m_framebufferManager;
    SwapChain& m_swapchain;

    // Type-based template registry
    std::unordered_map<std::type_index, std::unique_ptr<RenderGraphTemplate>> m_templates;

    // Instance cache and storage
    std::unordered_map<RenderGraphHandle, GraphResource> m_resources;

    // Two-level cache: [template_type][cache_key_hash] -> handle
    std::unordered_map<std::type_index, std::unordered_map<size_t, RenderGraphHandle>> m_graphCache;

    // Handle generation
    uint32_t m_nextHandleId = 1;

    // Internal methods
    template<typename GraphTemplateType>
    RenderGraphHandle createRenderGraph(const RenderTarget& renderTarget) {
        auto* templatePtr = getTemplate<GraphTemplateType>();
        if (!templatePtr) {
            throw std::runtime_error("Template not registered");
        }

        // Check compatibility
        if (!templatePtr->isCompatibleWithTarget(renderTarget)) {
            throw std::runtime_error("Render target not compatible with template");
        }

        auto extent = extractDimensions(renderTarget);
        auto graph = templatePtr->createRenderGraph(
            renderTarget, extent);

        if (!graph) {
            return RenderGraphHandle{}; // Invalid handle
        }

        auto handle = generateHandle();
        m_resources[handle] = GraphResource{ std::move(graph), 0, false };

        return handle;
    }

    RenderGraphHandle generateHandle();
    SmartHandle<RenderGraphHandle, RenderGraph> createSmartHandle(RenderGraphHandle handle);
};