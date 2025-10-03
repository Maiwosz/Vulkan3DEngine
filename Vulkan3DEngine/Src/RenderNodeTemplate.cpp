#include "RenderNodeTemplate.h"
#include "EngineCore.h"
#include "SwapChain.h"
#include "RenderPassManager.h"
#include "TextureManager.h"

SwapChain* RenderNodeTemplate::getSwapChain() const {
    return &m_engineCore.swapChain();
}

SmartRenderPassHandle RenderNodeTemplate::createSmartRenderPassHandle(
    const RenderPassConfig& config) const {
    return m_engineCore.renderPassManager().acquireSmartRenderPass(config);
}

VkFormat RenderNodeTemplate::getTextureFormat(const RenderTarget& target) const {
    if (target.isTexture()) {
        const auto& textureHandle = target.getTextureHandle();
        if (textureHandle.isValid()) {
            auto* textureInfo = textureHandle.get();
            return textureInfo ? Graphics::convertImageFormat(textureInfo->format) : VK_FORMAT_UNDEFINED;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

VkImageUsageFlags RenderNodeTemplate::getTextureUsageFlags(const RenderTarget& target) const {
    if (target.isTexture()) {
        const auto& textureHandle = target.getTextureHandle();
        if (textureHandle.isValid()) {
            auto* textureInfo = textureHandle.get();
            return textureInfo ? Graphics::convertImageUsage(textureInfo->usage) : 0;
        }
    }
    return 0;
}

bool RenderNodeTemplate::supportsMultisampling(const RenderTarget& target) const {
    return Graphics::convertSampleCount(target.getSampleCount()) > 1;
}