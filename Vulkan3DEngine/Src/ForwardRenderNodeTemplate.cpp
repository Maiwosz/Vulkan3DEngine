#include "ForwardRenderNodeTemplate.h"
#include "RenderNode.h"
#include "EngineCore.h"
#include <stdexcept>

ForwardRenderNodeTemplate::ForwardRenderNodeTemplate(EngineCore& engineCore)
    : RenderNodeTemplate(engineCore) {
}

bool ForwardRenderNodeTemplate::isCompatibleWithTarget(const RenderTarget& target) const {
    if (!target.isValid()) {
        return false;
    }
    return target.isSwapchain() || target.isTexture();
}

RenderNodeTemplate::RenderParameters ForwardRenderNodeTemplate::queryRenderParameters(
    const RenderTarget& target,
    VkExtent2D extent) const {

    const auto& templateInfo = getStaticTemplateInfo();
    RenderParameters params = {};
    params.extent = extent;
    params.hasColor = templateInfo.requiresColorBuffer;
    params.hasDepth = templateInfo.requiresDepthBuffer && m_config.enableDepthTesting;
    params.colorFormat = selectColorFormat(target);
    params.depthFormat = selectDepthFormat();
    params.samples = selectSampleCount(target);
    params.hasResolve = (params.samples > VK_SAMPLE_COUNT_1_BIT) && target.isSwapchain();
    params.initialLayout = selectInitialColorLayout(target);
    params.finalLayout = selectFinalColorLayout(target);

    if (target.isTexture()) {
        params.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (m_config.optimizeForTextures) {
            params.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
    }

    return params;
}

std::unique_ptr<RenderNode> ForwardRenderNodeTemplate::createRenderNode(
    const RenderTarget& target,
    VkExtent2D extent) const {

    if (!isCompatibleWithTarget(target)) {
        throw std::runtime_error("ForwardRenderNodeTemplate: Incompatible render target");
    }

    RenderParameters params = queryRenderParameters(target, extent);
    RenderPassConfig passConfig = createForwardRenderPassConfig(params);
    SmartRenderPassHandle smartRenderPass = createSmartRenderPassHandle(passConfig);

    if (!smartRenderPass.isValid()) {
        throw std::runtime_error("ForwardRenderNodeTemplate: Failed to create render pass");
    }

    auto renderNode = std::make_unique<RenderNode>(
        this,
        target,
        extent,
        std::move(smartRenderPass)
    );

    setupRenderNodeAttachments(*renderNode, params, passConfig);
    return renderNode;
}

RenderPassConfig ForwardRenderNodeTemplate::createForwardRenderPassConfig(
    const RenderParameters& params) const {

    auto& attachmentManager = m_engineCore.attachmentManager();
    RenderPassConfig config = {};
    uint32_t attachmentIndex = 0;

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
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE
        );

        config.attachments.push_back(colorAttachment);
        config.colorAttachmentIndices.push_back(attachmentIndex++);
    }

    if (params.hasDepth) {
        AttachmentImageSpec depthSpec = attachmentManager.getFactory().createDepthImageSpec(
            params.depthFormat,
            params.extent,
            params.samples
        );

        RenderPassAttachment depthAttachment = attachmentManager.getFactory().createDepthAttachment(
            depthSpec,
            0,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE
        );

        config.attachments.push_back(depthAttachment);
        config.depthAttachmentIndex = attachmentIndex++;
    }

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

    return config;
}

void ForwardRenderNodeTemplate::setupRenderNodeAttachments(
    RenderNode& node,
    const RenderParameters& params,
    const RenderPassConfig& passConfig) const {

    auto& attachmentManager = m_engineCore.attachmentManager();
    const RenderTarget& target = node.getRenderTarget();

    // Color attachments
    for (size_t i = 0; i < passConfig.colorAttachmentIndices.size(); ++i) {
        uint32_t attachmentIndex = passConfig.colorAttachmentIndices[i];
        const auto& attachment = passConfig.attachments[attachmentIndex];

        if (target.isSwapchain() && !params.hasResolve) {
            // Renderujemy bezpośrednio do swapchain - używamy swapchain attachment
            node.addSwapchainColorAttachment(attachmentIndex);
        }
        else {
            // MSAA lub texture target - używamy managed attachment
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

    // Depth attachment - zawsze managed
    if (passConfig.depthAttachmentIndex != UINT32_MAX) {
        const auto& attachment = passConfig.attachments[passConfig.depthAttachmentIndex];

        AttachmentImageSpec spec = {};
        spec.format = attachment.desc.format;
        spec.extent = params.extent;
        spec.samples = attachment.desc.samples;
        spec.type = AttachmentType::Depth;
        spec.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        AttachmentHandle handle = attachmentManager.acquireAttachment(spec);
        node.addDepthAttachment(handle, passConfig.depthAttachmentIndex);
    }

    // Resolve attachment
    if (passConfig.resolveAttachmentIndex != UINT32_MAX) {
        if (target.isSwapchain()) {
            // Resolve do swapchain
            node.addSwapchainResolveAttachment(passConfig.resolveAttachmentIndex);
        }
        else {
            // Resolve do managed attachment (texture target)
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
}

VkFormat ForwardRenderNodeTemplate::selectColorFormat(const RenderTarget& target) const {
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

VkFormat ForwardRenderNodeTemplate::selectDepthFormat() const {
    return getStaticTemplateInfo().preferredDepthFormat;
}

VkSampleCountFlagBits ForwardRenderNodeTemplate::selectSampleCount(const RenderTarget& target) const {
    if (!m_config.enableMSAA) {
        return VK_SAMPLE_COUNT_1_BIT;
    }
    return Graphics::convertSampleCount(target.getSampleCount(&m_engineCore.swapChain()));
}

VkImageLayout ForwardRenderNodeTemplate::selectInitialColorLayout(const RenderTarget& target) const {
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkImageLayout ForwardRenderNodeTemplate::selectFinalColorLayout(const RenderTarget& target) const {
    if (target.isSwapchain()) {
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    else if (target.isTexture()) {
        return m_config.optimizeForTextures ?
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}