#include "RenderGraphTemplate.h"
#include "EngineCore.h"
#include "RenderNodeManager.h"

// Template implementation for acquireNode
template<typename NodeTemplateType>
SmartHandle<RenderNodeHandle, RenderNode> RenderGraphTemplate::acquireNode(
    const RenderTarget& target) const {
    return m_engineCore.renderNodeManager().acquireSmartNode<NodeTemplateType>(target);
}

// Explicit template instantiations for known node template types
// Add more as needed when creating new node templates
#include "ForwardRenderNodeTemplate.h"
template SmartHandle<RenderNodeHandle, RenderNode>
RenderGraphTemplate::acquireNode<ForwardRenderNodeTemplate>(const RenderTarget&) const;