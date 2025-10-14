#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <functional>
#include "RenderNodeTemplate.h"

// Forward declarations
class RenderTarget;
class RenderGraph;
class AttachmentManager;
class RenderPassManager;
class SwapChain;

/**
 * Connection between render nodes - describes data flow from output to input.
 */
struct NodeConnection {
    uint32_t sourceNodeIndex;      // Index of source node in template's node list
    uint32_t sourceOutputIndex;    // Index of output slot in source node
    uint32_t targetNodeIndex;      // Index of target node in template's node list
    uint32_t targetInputIndex;     // Index of input slot in target node

    NodeConnection(uint32_t srcNode, uint32_t srcOutput,
        uint32_t dstNode, uint32_t dstInput)
        : sourceNodeIndex(srcNode)
        , sourceOutputIndex(srcOutput)
        , targetNodeIndex(dstNode)
        , targetInputIndex(dstInput) {
    }
};

/**
 * Dynamically buildable render graph template.
 * Can be constructed programmatically or loaded from files.
 */
class RenderGraphTemplate {
public:
    RenderGraphTemplate() = default;
    explicit RenderGraphTemplate(std::string name) : m_name(std::move(name)) {}

    // Builder-style methods
    RenderGraphTemplate& setName(std::string name) {
        m_name = std::move(name);
        return *this;
    }

    /**
     * Add a node template to the graph.
     * Returns the index of the added node for use in connections.
     */
    uint32_t addNode(std::unique_ptr<RenderNodeTemplate> nodeTemplate) {
        uint32_t index = static_cast<uint32_t>(m_nodeTemplates.size());
        m_nodeTemplates.push_back(std::move(nodeTemplate));
        return index;
    }

    /**
     * Connect output of one node to input of another.
     */
    RenderGraphTemplate& connect(uint32_t sourceNode, uint32_t sourceOutput,
        uint32_t targetNode, uint32_t targetInput) {
        m_connections.emplace_back(sourceNode, sourceOutput, targetNode, targetInput);
        return *this;
    }

    /**
     * Set target compatibility predicate.
     * By default, compatible with all targets.
     */
    RenderGraphTemplate& setTargetCompatibilityCheck(
        std::function<bool(const RenderTarget&)> checker) {
        m_targetCompatibilityChecker = std::move(checker);
        return *this;
    }

    // Accessors
    const std::string& getName() const { return m_name; }

    const std::vector<std::unique_ptr<RenderNodeTemplate>>& getNodeTemplates() const {
        return m_nodeTemplates;
    }

    const std::vector<NodeConnection>& getConnections() const {
        return m_connections;
    }

    uint32_t getNodeCount() const {
        return static_cast<uint32_t>(m_nodeTemplates.size());
    }

    uint32_t getConnectionCount() const {
        return static_cast<uint32_t>(m_connections.size());
    }

    // Get non-owning pointers for compatibility with existing RenderGraph
    std::vector<const RenderNodeTemplate*> getNodeTemplatePointers() const {
        std::vector<const RenderNodeTemplate*> ptrs;
        ptrs.reserve(m_nodeTemplates.size());
        for (const auto& tmpl : m_nodeTemplates) {
            ptrs.push_back(tmpl.get());
        }
        return ptrs;
    }

    /**
     * Check if this template is compatible with given render target.
     */
    bool isCompatibleWithTarget(const RenderTarget& target) const {
        if (m_targetCompatibilityChecker) {
            return m_targetCompatibilityChecker(target);
        }
        return true; // Default: compatible with all targets
    }

    /**
     * Create a concrete RenderGraph instance from this template.
     */
    std::unique_ptr<RenderGraph> createRenderGraph(
        const RenderTarget& target,
        AttachmentManager& attachmentMgr,
        RenderPassManager& renderPassMgr) const;

    // Validation
    bool validate() const {
        if (m_name.empty() || m_nodeTemplates.empty()) {
            return false;
        }

        // Validate all connections
        for (const auto& conn : m_connections) {
            if (!validateConnection(conn)) {
                return false;
            }
        }

        // Validate all nodes
        for (const auto& node : m_nodeTemplates) {
            if (!node || !node->isValid()) {
                return false;
            }
        }

        return true;
    }

private:
    bool validateConnection(const NodeConnection& connection) const {
        // Check node indices
        if (connection.sourceNodeIndex >= m_nodeTemplates.size() ||
            connection.targetNodeIndex >= m_nodeTemplates.size()) {
            return false;
        }

        const RenderNodeTemplate* sourceNode = m_nodeTemplates[connection.sourceNodeIndex].get();
        const RenderNodeTemplate* targetNode = m_nodeTemplates[connection.targetNodeIndex].get();

        if (!sourceNode || !targetNode) {
            return false;
        }

        // Check slot indices
        const auto& sourceSpec = sourceNode->getAttachmentSpec();
        const auto& targetSpec = targetNode->getAttachmentSpec();

        if (connection.sourceOutputIndex >= sourceSpec.getOutputCount() ||
            connection.targetInputIndex >= targetSpec.getInputCount()) {
            return false;
        }

        return true;
    }

    std::string m_name;
    std::vector<std::unique_ptr<RenderNodeTemplate>> m_nodeTemplates;
    std::vector<NodeConnection> m_connections;
    std::function<bool(const RenderTarget&)> m_targetCompatibilityChecker;
};