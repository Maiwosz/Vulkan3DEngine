#pragma once
#include <vulkan/vulkan.h>
#include "RenderNodeTemplate.h"
#include "Handle.h"
#include "RenderPassManager.h"

using SmartRenderPassHandle = SmartHandle<RenderPassHandle, VkRenderPass>;

/**
 * Lightweight descriptor of a render node instance.
 * Only holds template reference and render pass - NO attachments, NO framebuffers.
 */
class RenderNode {
public:
    RenderNode(const RenderNodeTemplate* nodeTemplate,
        SmartRenderPassHandle renderPass,
        VkExtent2D extent);

    const RenderNodeTemplate* getTemplate() const { return m_template; }
    VkExtent2D getExtent() const { return m_extent; }
    VkRenderPass getRenderPass() const { return *m_renderPass.get(); }
    const SmartRenderPassHandle& getSmartRenderPassHandle() const { return m_renderPass; }

    bool isValid() const;

private:
    const RenderNodeTemplate* m_template;
    SmartRenderPassHandle m_renderPass;
    VkExtent2D m_extent;
};