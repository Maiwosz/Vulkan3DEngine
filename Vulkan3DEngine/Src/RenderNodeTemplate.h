#pragma once
#include "RenderTypes.h"
#include "RenderNode.h"
#include "RenderPassManager.h"
#include "AttachmentManager.h"
#include <memory>

// Base class for render node templates
class RenderNodeTemplate {
public:
    virtual ~RenderNodeTemplate() = default;

    // Template identification
    virtual RenderTemplateType getTemplateType() const = 0;

    // Compatibility check
    virtual bool isCompatible(const RenderPassMetadata& metadata) const = 0;

    // Create a render node instance for the given metadata
    virtual std::unique_ptr<RenderNode> createRenderNode(
        RenderPassManager& renderPassManager,
        AttachmentManager& attachmentManager,
        const RenderPassMetadata& metadata) const = 0;

protected:
    RenderNodeTemplate() = default;
};