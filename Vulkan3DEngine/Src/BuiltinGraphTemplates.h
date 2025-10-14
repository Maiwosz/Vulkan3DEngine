#pragma once
#include "RenderGraphTemplate.h"
#include "RenderNodeTemplate.h"
#include <memory>

/**
 * Factory for built-in render graph templates.
 * Provides commonly used rendering configurations.
 */
namespace BuiltinGraphTemplates {

    /**
     * Create a forward rendering template with depth buffer.
     * Single pass with color and depth outputs.
     *
     * Structure:
     * - Node 0: Forward rendering pass with depth
     *   - Output 0: Color attachment
     *   - Output 1: Depth attachment
     *
     * @param name Optional name for the template (default: "ForwardRendering")
     * @return Configured RenderGraphTemplate ready to use
     */
    inline std::unique_ptr<RenderGraphTemplate> createForwardRendering(
        const std::string& name = "ForwardRendering") {

        auto graphTemplate = std::make_unique<RenderGraphTemplate>(name);

        auto forwardNode = std::make_unique<RenderNodeTemplate>("ForwardPass");
        forwardNode->addOutputAttachment(AttachmentSlot::Role::Color, "ColorOutput");
        forwardNode->addOutputAttachment(AttachmentSlot::Role::Depth, "DepthOutput");

        graphTemplate->addNode(std::move(forwardNode));

        return graphTemplate;
    }

} // namespace BuiltinGraphTemplates