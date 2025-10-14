#include "RenderGraphTemplate.h"
#include "RenderGraph.h"
#include "RenderGraphBuilder.h"
#include "RenderTarget.h"

std::unique_ptr<RenderGraph> RenderGraphTemplate::createRenderGraph(
    const RenderTarget& target,
    AttachmentManager& attachmentMgr,
    RenderPassManager& renderPassMgr) const {

    if (!validate()) {
        throw std::runtime_error("RenderGraphTemplate: invalid template");
    }

    if (!isCompatibleWithTarget(target)) {
        throw std::runtime_error("RenderGraphTemplate: incompatible with target");
    }

    // Use builder to construct the concrete graph
    RenderGraphBuilder builder(*this, target, attachmentMgr, renderPassMgr);
    return builder.build();
}