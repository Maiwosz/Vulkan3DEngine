#pragma once
#include "RenderTypes.h"
#include "RenderTarget.h"
#include "RenderNode.h"
#include "Handle.h"
#include <vector>
#include <typeindex>

// Forward declarations
class RenderGraphTemplate;
class RenderOrder;

/**
 * Simple render graph - sequence of render nodes with dynamic configuration.
 * Now uses type-based template identification instead of enums.
 */

 // Execution node - pairs render node handle with draw orders
struct RenderGraphNode {
    SmartHandle<RenderNodeHandle, RenderNode> renderNodeHandle;  // Smart handle to cached node
    std::vector<RenderOrder*> renderOrders;  // What to draw in this node

    explicit RenderGraphNode(SmartHandle<RenderNodeHandle, RenderNode> handle)
        : renderNodeHandle(std::move(handle)) {
    }
};

// Complete render graph - sequential execution of nodes with dynamic configuration
class RenderGraph {
public:
    RenderGraph(std::type_index templateTypeIndex,
        const RenderTarget& renderTarget,
        VkExtent2D extent);

    virtual ~RenderGraph() = default;

    // Non-copyable but movable
    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) = default;
    RenderGraph& operator=(RenderGraph&&) = default;

    // Basic properties
    std::type_index getTemplateTypeIndex() const { return m_templateTypeIndex; }
    const RenderTarget& getRenderTarget() const { return m_renderTarget; }
    VkExtent2D getExtent() const { return m_extent; }

    // Template type checking
    template<typename TemplateType>
    bool isTemplateType() const {
        return m_templateTypeIndex == std::type_index(typeid(TemplateType));
    }

    // Node access
    const std::vector<RenderGraphNode>& getNodes() const { return m_nodes; }
    size_t getNodeCount() const { return m_nodes.size(); }
    RenderGraphNode* getNode(size_t index);
    const RenderGraphNode* getNode(size_t index) const;

    // Setup interface - used by templates during creation
    void addNode(SmartHandle<RenderNodeHandle, RenderNode> nodeHandle);
    void addOrderToNode(size_t nodeIndex, RenderOrder* order);

    // Clear all render orders (for dynamic reconfiguration)
    void clearOrders();

private:
    std::type_index m_templateTypeIndex;        // Template type used to create this graph
    RenderTarget m_renderTarget;                // Target specification
    VkExtent2D m_extent;                       // Cached dimensions
    std::vector<RenderGraphNode> m_nodes;      // Sequence of execution nodes

    friend class RenderGraphTemplate;  // Allow template to modify during creation
};