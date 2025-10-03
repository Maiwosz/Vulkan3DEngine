#pragma once
#include "RenderTypes.h"
#include "RenderNode.h"
#include "RenderNodeTemplate.h"
#include "ISmartHandleManager.h"
#include "RenderTarget.h"
#include <memory>
#include <unordered_map>
#include <typeindex>

// Forward declarations
class EngineCore;
class RenderPassManager;
class AttachmentManager;
class SwapChain;

/**
 * Render node instance cache and factory with type-based template system.
 *
 * Architecture Role:
 * - Caches render nodes by (template type, target type, dimensions)
 * - Manages templates using compile-time type information
 * - Provides smart handle system for automatic resource management
 * - Reuses compatible nodes across multiple render graphs
 *
 * Now uses template metaprogramming instead of enum-based registration.
 */
class RenderNodeManager : public ISmartHandleManager<RenderNodeHandle, RenderNode> {
public:
    RenderNodeManager(EngineCore& engineCore);
    ~RenderNodeManager();

    // Non-copyable
    RenderNodeManager(const RenderNodeManager&) = delete;
    RenderNodeManager& operator=(const RenderNodeManager&) = delete;

    // ISmartHandleManager interface
    RenderNode* getResource(RenderNodeHandle handle) override;
    bool isValid(RenderNodeHandle handle) const override;
    void releaseResource(RenderNodeHandle handle) override;
    void addReference(RenderNodeHandle handle) override;
    void removeReference(RenderNodeHandle handle) override;

    // Type-based template management - no runtime registration needed
    template<typename TemplateType>
    bool hasTemplate() const {
        return m_templates.find(std::type_index(typeid(TemplateType))) != m_templates.end();
    }

    template<typename TemplateType>
    void registerTemplate(std::unique_ptr<TemplateType> nodeTemplate) {
        static_assert(std::is_base_of_v<RenderNodeTemplate, TemplateType>,
            "TemplateType must derive from RenderNodeTemplate");
        m_templates[std::type_index(typeid(TemplateType))] = std::move(nodeTemplate);
    }

    template<typename TemplateType>
    TemplateType* getTemplate() const {
        auto it = m_templates.find(std::type_index(typeid(TemplateType)));
        if (it != m_templates.end()) {
            return static_cast<TemplateType*>(it->second.get());
        }
        return nullptr;
    }

    // Node acquisition with automatic template creation if not registered
    template<typename TemplateType>
    SmartHandle<RenderNodeHandle, RenderNode> acquireSmartNode(const RenderTarget& renderTarget) {
        // Auto-register template if not present
        if (!hasTemplate<TemplateType>()) {
            registerTemplate(std::make_unique<TemplateType>(m_engineCore));
        }

        auto* node = getNode<TemplateType>(renderTarget);
        if (!node) {
            return SmartHandle<RenderNodeHandle, RenderNode>(); // Invalid handle
        }

        // Find the handle for this node
        for (const auto& [handle, resource] : m_resources) {
            if (resource.node.get() == node) {
                return createSmartHandle(handle);
            }
        }

        return SmartHandle<RenderNodeHandle, RenderNode>(); // Should not happen
    }

    // Direct access (for cases not needing smart handles)
    template<typename TemplateType>
    RenderNode* getNode(const RenderTarget& renderTarget) {
        // Validate render target
        if (!renderTarget.isValid()) {
            throw std::invalid_argument("Invalid render target");
        }

        // Extract dimensions directly from render target
        VkExtent2D extent = renderTarget.getDimensions(&m_swapChain);

        // Create cache key
        RenderNodeCacheKey<TemplateType> cacheKey;
        cacheKey.targetType = renderTarget.getType();
        cacheKey.extent = extent;

        // Look for existing cached node
        auto cacheIt = m_nodeCache.find(std::type_index(typeid(TemplateType)));
        if (cacheIt != m_nodeCache.end()) {
            auto& typeCache = cacheIt->second;
            auto keyHash = std::hash<RenderNodeCacheKey<TemplateType>>{}(cacheKey);
            auto nodeIt = typeCache.find(keyHash);
            if (nodeIt != typeCache.end()) {
                auto resourceIt = m_resources.find(nodeIt->second);
                if (resourceIt != m_resources.end() && !resourceIt->second.markedForDeletion) {
                    return resourceIt->second.node.get();
                }
                else {
                    // Handle became invalid, remove from cache
                    typeCache.erase(nodeIt);
                }
            }
        }

        // Create new node if not found
        auto handle = createRenderNode<TemplateType>(renderTarget);
        if (!handle.isValid()) {
            return nullptr;
        }

        // Cache it
        auto keyHash = std::hash<RenderNodeCacheKey<TemplateType>>{}(cacheKey);
        m_nodeCache[std::type_index(typeid(TemplateType))][keyHash] = handle;

        return getResource(handle);
    }

    // Cache management
    void clearCache();
    size_t getCacheSize() const;

private:
    struct NodeResource {
        std::unique_ptr<RenderNode> node;
        uint32_t referenceCount = 0;
        bool markedForDeletion = false;
    };

    // Dependencies
	EngineCore& m_engineCore;
    RenderPassManager& m_renderPassManager;
    AttachmentManager& m_attachmentManager;
    SwapChain& m_swapChain;

    // Type-based template registry
    std::unordered_map<std::type_index, std::unique_ptr<RenderNodeTemplate>> m_templates;

    // Instance cache and storage
    std::unordered_map<RenderNodeHandle, NodeResource> m_resources;

    // Two-level cache: [template_type][cache_key_hash] -> handle
    std::unordered_map<std::type_index, std::unordered_map<size_t, RenderNodeHandle>> m_nodeCache;

    // Handle generation
    uint32_t m_nextHandleId = 1;

    // Internal methods
    template<typename TemplateType>
    RenderNodeHandle createRenderNode(const RenderTarget& renderTarget) {
        auto* templatePtr = getTemplate<TemplateType>();
        if (!templatePtr) {
            throw std::runtime_error("Template not registered");
        }

        // Check compatibility
        if (!templatePtr->isCompatibleWithTarget(renderTarget)) {
            throw std::runtime_error("Render target not compatible with template");
        }

        auto extent = renderTarget.getDimensions(&m_swapChain);
        auto node = templatePtr->createRenderNode(
            renderTarget, extent);

        if (!node) {
            return RenderNodeHandle{}; // Invalid handle
        }

        // Verify the created node is complete
        if (!node->isComplete()) {
            throw std::runtime_error("Created render node is incomplete");
        }

        auto handle = generateHandle();
        m_resources[handle] = NodeResource{ std::move(node), 0, false };

        return handle;
    }

    RenderNodeHandle generateHandle();
    SmartHandle<RenderNodeHandle, RenderNode> createSmartHandle(RenderNodeHandle handle);
};