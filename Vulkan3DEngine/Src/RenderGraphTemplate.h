#pragma once
#include "RenderGraph.h"
#include <memory>
#include <typeindex>
#include "RenderNodeManager.h"

// Forward declarations
class EngineCore;

/**
 * Abstract factory for creating render graphs with access to engine systems.
 *
 * Architecture Role:
 * - Contains static metadata about graph type
 * - Factory method creates configured RenderGraph for specific target
 * - Has access to EngineCore for retrieving necessary managers
 * - Orchestrates RenderNodeManager to get individual nodes
 */
class RenderGraphTemplate {
public:
    explicit RenderGraphTemplate(EngineCore& engineCore);

    virtual ~RenderGraphTemplate() = default;

    // Static template metadata
    virtual const RenderGraphTemplateInfo& getTemplateInfo() const = 0;

    // Type information
    virtual std::type_index getTypeIndex() const {
        return std::type_index(typeid(*this));
    }

    const char* getTemplateName() const { return getTemplateInfo().name; }

    // Capability check
    virtual bool isCompatibleWithTarget(const RenderTarget& target) const = 0;

    // Factory method - simplified interface using EngineCore internally
    virtual std::unique_ptr<RenderGraph> createRenderGraph(
        const RenderTarget& target,
        VkExtent2D extent) const = 0;

protected:
    EngineCore& m_engineCore;
    RenderNodeManager& m_nodeManager;

    std::unique_ptr<RenderGraph> createBaseGraph(
        const RenderTarget& target,
        VkExtent2D extent) const {
        return std::make_unique<RenderGraph>(getTypeIndex(), target, extent);
    }

    // Implementacja inline w nagłówku
    template<typename NodeTemplateType>
    SmartHandle<RenderNodeHandle, RenderNode> acquireNode(
        const RenderTarget& target) const {
        return m_nodeManager.acquireSmartNode<NodeTemplateType>(target);
    }
};