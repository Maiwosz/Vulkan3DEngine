#pragma once
#include "IAssetHandler.h"
#include "TextureManager.h"
#include <optional>
#include <variant>
#include "SwapChain.h"

// Forward declarations
class SwapChain;

/**
 * Modern render target using smart handles for automatic resource management.
 * Eliminates the need to pass TextureManager references around.
 */
class RenderTarget {
public:
    // Target type enumeration
    enum class Type {
        SwapChain,
        Texture
    };

    // Constructors
    static RenderTarget createSwapChainTarget() {
        return RenderTarget(Type::SwapChain);
    }

    static RenderTarget createTextureTarget(const SmartTextureHandle& textureHandle) {
        return RenderTarget(textureHandle);
    }

    // Type queries
    bool isSwapchain() const { return m_type == Type::SwapChain; }
    bool isTexture() const { return m_type == Type::Texture; }

    Type getType() const { return m_type; }

    // Smart handle access
    const SmartTextureHandle& getTextureHandle() const {
        if (!isTexture()) {
            throw std::runtime_error("RenderTarget is not a texture target");
        }
        return m_textureHandle;
    }

    // Dimension extraction - no external managers needed
    VkExtent2D getDimensions(SwapChain* swapChain = nullptr) const {
        if (isSwapchain()) {
            if (!swapChain) {
                throw std::runtime_error("SwapChain required for swapchain targets");
            }
            return swapChain->getSwapChainExtent();
        }
        else {
            if (!m_textureHandle.isValid()) {
                throw std::runtime_error("Invalid texture handle in render target");
            }
            auto* textureInfo = m_textureHandle.get();
            return VkExtent2D{ textureInfo->width, textureInfo->height };
        }
    }

    Settings::MsaaSampleCount getSampleCount(SwapChain* swapChain = nullptr) const {
        if (isSwapchain()) {
            if (!swapChain) {
                throw std::runtime_error("SwapChain required for swapchain targets");
            }
            return swapChain->getMsaaSamples();
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
    bool isValid() const {
        if (isSwapchain()) {
            return true; // SwapChain targets are always considered valid
        }
        else {
            return m_textureHandle.isValid();
        }
    }

    // Comparison operators for caching
    bool operator==(const RenderTarget& other) const {
        if (m_type != other.m_type) return false;

        if (isSwapchain()) {
            return true; // All swapchain targets are equivalent
        }
        else {
            return m_textureHandle.handle() == other.m_textureHandle.handle();
        }
    }

    bool operator!=(const RenderTarget& other) const {
        return !(*this == other);
    }

private:
    // Private constructors to enforce factory methods
    explicit RenderTarget(Type type)
        : m_type(type) {
        if (type != Type::SwapChain) {
            throw std::invalid_argument("This constructor is only for SwapChain targets");
        }
    }

    explicit RenderTarget(const SmartTextureHandle& textureHandle)
        : m_type(Type::Texture), m_textureHandle(textureHandle) {
        if (!textureHandle.isValid()) {
            throw std::invalid_argument("Cannot create RenderTarget with invalid texture handle");
        }
    }

    Type m_type;
    SmartTextureHandle m_textureHandle; // Only valid when m_type == Type::Texture
};
