#include "RenderTarget.h"
#include "SwapChain.h"
#include <stdexcept>

// Factory methods
RenderTarget RenderTarget::createSwapChainTarget(SwapChain* swapChain) {
    if (!swapChain) {
        throw std::invalid_argument("SwapChain pointer cannot be null");
    }
    return RenderTarget(swapChain);
}

RenderTarget RenderTarget::createTextureTarget(const SmartTextureHandle& textureHandle) {
    return RenderTarget(textureHandle);
}

// Smart handle access
const SmartTextureHandle& RenderTarget::getTextureHandle() const {
    if (!isTexture()) {
        throw std::runtime_error("RenderTarget is not a texture target");
    }
    return m_textureHandle;
}

// SwapChain access
SwapChain* RenderTarget::getSwapChain() const {
    if (!isSwapchain()) {
        throw std::runtime_error("RenderTarget is not a swapchain target");
    }
    return m_swapChain;
}

// Dimension extraction
VkExtent2D RenderTarget::getDimensions() const {
    if (isInvalid()) {
        throw std::runtime_error("Cannot get dimensions from invalid render target");
    }

    if (isSwapchain()) {
        return m_swapChain->getSwapChainExtent();
    }
    else {
        if (!m_textureHandle.isValid()) {
            throw std::runtime_error("Invalid texture handle in render target");
        }
        auto* textureInfo = m_textureHandle.get();
        return VkExtent2D{ textureInfo->width, textureInfo->height };
    }
}

Settings::MsaaSampleCount RenderTarget::getSampleCount() const {
    if (isInvalid()) {
        throw std::runtime_error("Cannot get sample count from invalid render target");
    }

    if (isSwapchain()) {
        return m_swapChain->getMsaaSamples();
    }
    else {
        if (!m_textureHandle.isValid()) {
            throw std::runtime_error("Invalid texture handle in render target");
        }
        auto* textureInfo = m_textureHandle.get();
        return textureInfo->samples;
    }
}

// Validity check
bool RenderTarget::isValid() const {
    if (isInvalid()) {
        return false;
    }

    if (isSwapchain()) {
        return m_swapChain != nullptr;
    }
    else {
        return m_textureHandle.isValid();
    }
}

// Comparison operators
bool RenderTarget::operator==(const RenderTarget& other) const {
    if (m_type != other.m_type) return false;

    if (isInvalid()) {
        return true; // All invalid targets are equal
    }

    if (isSwapchain()) {
        return m_swapChain == other.m_swapChain;
    }
    else {
        return m_textureHandle.handle() == other.m_textureHandle.handle();
    }
}

bool RenderTarget::operator!=(const RenderTarget& other) const {
    return !(*this == other);
}

// Hash function
size_t RenderTarget::hash() const {
    size_t h = std::hash<int>{}(static_cast<int>(m_type));

    if (isInvalid()) {
        return h;
    }

    if (isSwapchain()) {
        size_t h2 = std::hash<const void*>{}(static_cast<const void*>(m_swapChain));
        return h ^ (h2 << 1);
    }
    else {
        size_t h2 = std::hash<uint32_t>{}(m_textureHandle.handle().id);
        return h ^ (h2 << 1);
    }
}

// Private constructors
RenderTarget::RenderTarget(SwapChain* swapChain)
    : m_type(Type::SwapChain), m_swapChain(swapChain) {
}

RenderTarget::RenderTarget(const SmartTextureHandle& textureHandle)
    : m_type(Type::Texture), m_textureHandle(textureHandle), m_swapChain(nullptr) {
    if (!textureHandle.isValid()) {
        throw std::invalid_argument("Cannot create RenderTarget with invalid texture handle");
    }
}