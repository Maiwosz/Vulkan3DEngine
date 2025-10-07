#include "ForwardRenderGraphTemplate.h"
#include "RenderGraph.h"
#include "EngineCore.h"
#include "ForwardRenderNodeTemplate.h"
#include "ImGuiRenderNodeTemplate.h"
#include <stdexcept>

ForwardRenderGraphTemplate::ForwardRenderGraphTemplate(EngineCore& engineCore)
    : RenderGraphTemplate(engineCore) {
}

const RenderGraphTemplateInfo& ForwardRenderGraphTemplate::getTemplateInfo() const {
    return getStaticTemplateInfo();
}

bool ForwardRenderGraphTemplate::isCompatibleWithTarget(const RenderTarget& target) const {
    return validateTargetCompatibility(target);
}

std::unique_ptr<RenderGraph> ForwardRenderGraphTemplate::createRenderGraph(
    const RenderTarget& target,
    VkExtent2D extent) const {

    if (!isCompatibleWithTarget(target)) {
        throw std::runtime_error("ForwardRenderGraphTemplate: Target is not compatible with forward rendering");
    }

    auto renderGraph = createBaseGraph(target, extent);

    // Step 1: Add forward rendering node
    // Node templates are pre-configured during registration
    auto forwardNodeHandle = acquireNode<ForwardRenderNodeTemplate>(target);
    if (!forwardNodeHandle.isValid()) {
        throw std::runtime_error("ForwardRenderGraphTemplate: Failed to acquire ForwardRenderNode");
    }
    renderGraph->addNode(std::move(forwardNodeHandle));

    // Step 2: Optionally add ImGui overlay node
    if (m_config.enableImGui) {
        auto imguiNodeHandle = acquireNode<ImGuiRenderNodeTemplate>(target);
        if (!imguiNodeHandle.isValid()) {
            throw std::runtime_error("ForwardRenderGraphTemplate: Failed to acquire ImGuiRenderNode");
        }
        renderGraph->addNode(std::move(imguiNodeHandle));
    }

    return renderGraph;
}

bool ForwardRenderGraphTemplate::validateTargetCompatibility(const RenderTarget& target) const {
    switch (target.getType()) {
    case RenderTarget::Type::SwapChain:
        return true;

    case RenderTarget::Type::Texture:
        if (target.isTexture()) {
            const auto& textureHandle = target.getTextureHandle();
            if (!textureHandle.isValid()) {
                return false;
            }

            auto* textureInfo = textureHandle.get();
            if (!textureInfo) {
                return false;
            }

            VkImageUsageFlags usageFlags = Graphics::convertImageUsage(textureInfo->usage);
            if (!(usageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
                return false;
            }

            VkFormat format = Graphics::convertImageFormat(textureInfo->format);
            if (format == VK_FORMAT_D32_SFLOAT ||
                format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                format == VK_FORMAT_D24_UNORM_S8_UINT ||
                format == VK_FORMAT_X8_D24_UNORM_PACK32) {
                return false;
            }

            return true;
        }
        return false;

    default:
        return false;
    }
}