#include "ImGuiRenderNodeTemplate.h"
#include "ImGuiRenderNode.h"
#include "EngineCore.h"
#include <stdexcept>

ImGuiRenderNodeTemplate::ImGuiRenderNodeTemplate(EngineCore& engineCore)
    : RenderNodeTemplate(engineCore) {
}

bool ImGuiRenderNodeTemplate::isCompatibleWithTarget(const RenderTarget& target) const {
    if (!target.isValid()) {
        return false;
    }
    // ImGui can render to swapchain or texture targets
    return target.isSwapchain() || target.isTexture();
}

RenderNodeTemplate::RenderParameters ImGuiRenderNodeTemplate::queryRenderParameters(
    const RenderTarget& target,
    VkExtent2D extent) const {

    const auto& templateInfo = getStaticTemplateInfo();
    RenderParameters params = {};
    params.extent = extent;
    params.hasColor = templateInfo.requiresColorBuffer;
    params.hasDepth = false; // ImGui never needs depth
    params.colorFormat = selectColorFormat(target);
    params.depthFormat = VK_FORMAT_UNDEFINED;
    params.samples = selectSampleCount(target);
    params.hasResolve = (params.samples > VK_SAMPLE_COUNT_1_BIT) && target.isSwapchain();

    // ImGui always loads existing content (renders on top)
    params.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    params.finalLayout = target.isSwapchain() ?
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR :
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if (target.isTexture()) {
        params.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    return params;
}

std::unique_ptr<RenderNode> ImGuiRenderNodeTemplate::createRenderNode(
    const RenderTarget& target,
    VkExtent2D extent) const {

    if (!isCompatibleWithTarget(target)) {
        throw std::runtime_error("ImGuiRenderNodeTemplate: Incompatible render target");
    }

    RenderParameters params = queryRenderParameters(target, extent);
    RenderPassConfig passConfig = createImGuiRenderPassConfig(params);
    SmartRenderPassHandle smartRenderPass = createSmartRenderPassHandle(passConfig);

    if (!smartRenderPass.isValid()) {
        throw std::runtime_error("ImGuiRenderNodeTemplate: Failed to create render pass");
    }

    // Create specialized ImGuiRenderNode - much simpler constructor
    auto renderNode = std::make_unique<ImGuiRenderNode>(
        this,
        target,
        extent,
        std::move(smartRenderPass),
        m_engineCore.imguiCore(),
        params.samples  // Pass MSAA samples from params
    );

    setupRenderNodeAttachments(*renderNode, params, passConfig);
    return renderNode;
}

RenderPassConfig ImGuiRenderNodeTemplate::createImGuiRenderPassConfig(
    const RenderParameters& params) const {

    auto& attachmentManager = m_engineCore.attachmentManager();
    RenderPassConfig config = {};
    uint32_t attachmentIndex = 0;

    // Color attachment - always present for ImGui
    if (params.hasColor) {
        AttachmentImageSpec colorSpec = attachmentManager.getFactory().createColorImageSpec(
            params.colorFormat,
            params.extent,
            params.samples,
            params.usage
        );

        RenderPassAttachment colorAttachment = attachmentManager.getFactory().createColorAttachment(
            colorSpec,
            0,
            params.initialLayout,
            params.hasResolve ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : params.finalLayout,
            m_config.loadOp,  // LOAD - preserve existing content
            VK_ATTACHMENT_STORE_OP_STORE
        );

        config.attachments.push_back(colorAttachment);
        config.colorAttachmentIndices.push_back(attachmentIndex++);
    }

    // Resolve attachment for MSAA
    if (params.hasResolve) {
        AttachmentImageSpec resolveSpec = attachmentManager.getFactory().createResolveImageSpec(
            params.colorFormat,
            params.extent
        );

        RenderPassAttachment resolveAttachment = attachmentManager.getFactory().createResolveAttachment(
            resolveSpec,
            0,
            VK_IMAGE_LAYOUT_UNDEFINED,
            params.finalLayout,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_STORE
        );

        config.attachments.push_back(resolveAttachment);
        config.resolveAttachmentIndex = attachmentIndex++;
    }

    // No depth attachment for ImGui
    config.depthAttachmentIndex = UINT32_MAX;

    return config;
}

void ImGuiRenderNodeTemplate::setupRenderNodeAttachments(
    ImGuiRenderNode& node,
    const RenderParameters& params,
    const RenderPassConfig& passConfig) const {

    auto& attachmentManager = m_engineCore.attachmentManager();
    const RenderTarget& target = node.getRenderTarget();

    // Color attachments
    for (size_t i = 0; i < passConfig.colorAttachmentIndices.size(); ++i) {
        uint32_t attachmentIndex = passConfig.colorAttachmentIndices[i];
        const auto& attachment = passConfig.attachments[attachmentIndex];

        if (target.isSwapchain() && !params.hasResolve) {
            // Render directly to swapchain
            node.addSwapchainColorAttachment(attachmentIndex);
        }
        else {
            // MSAA or texture target - use managed attachment
            AttachmentImageSpec spec = {};
            spec.format = attachment.desc.format;
            spec.extent = params.extent;
            spec.samples = attachment.desc.samples;
            spec.type = AttachmentType::Color;
            spec.usage = params.usage;

            AttachmentHandle handle = attachmentManager.acquireAttachment(spec);
            node.addColorAttachment(handle, attachmentIndex);
        }
    }

    // Resolve attachment
    if (passConfig.resolveAttachmentIndex != UINT32_MAX) {
        if (target.isSwapchain()) {
            // Resolve to swapchain
            node.addSwapchainResolveAttachment(passConfig.resolveAttachmentIndex);
        }
        else {
            // Resolve to managed attachment (texture target)
            const auto& attachment = passConfig.attachments[passConfig.resolveAttachmentIndex];

            AttachmentImageSpec spec = {};
            spec.format = attachment.desc.format;
            spec.extent = params.extent;
            spec.samples = VK_SAMPLE_COUNT_1_BIT;
            spec.type = AttachmentType::Resolve;
            spec.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            AttachmentHandle handle = attachmentManager.acquireAttachment(spec);
            node.addResolveAttachment(handle, passConfig.resolveAttachmentIndex);
        }
    }

    // No depth attachment - ImGui doesn't use depth testing
}

VkFormat ImGuiRenderNodeTemplate::selectColorFormat(const RenderTarget& target) const {
    if (target.isSwapchain()) {
        SwapChain* swapChain = getSwapChain();
        if (swapChain) {
            return swapChain->getImageFormat();
        }
        return getStaticTemplateInfo().preferredColorFormat;
    }
    else if (target.isTexture()) {
        VkFormat textureFormat = getTextureFormat(target);
        if (textureFormat != VK_FORMAT_UNDEFINED) {
            return textureFormat;
        }
    }
    return getStaticTemplateInfo().preferredColorFormat;
}

VkSampleCountFlagBits ImGuiRenderNodeTemplate::selectSampleCount(const RenderTarget& target) const {
    if (!m_config.enableMSAA) {
        return VK_SAMPLE_COUNT_1_BIT;
    }
    return Graphics::convertSampleCount(target.getSampleCount(&m_engineCore.swapChain()));
}