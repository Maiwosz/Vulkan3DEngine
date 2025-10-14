#pragma once
#include "IAssetHandler.h"
#include "TextureManager.h"
#include <optional>
#include <variant>
#include <functional>

// Forward declarations
class SwapChain;

/**
 * Modern render target using smart handles for automatic resource management.
 * Holds reference to SwapChain when applicable, eliminating the need to pass it separately.
 */
class RenderTarget {
public:
    // Target type enumeration
    enum class Type {
        Invalid,
        SwapChain,
        Texture
    };

    // Default constructor - creates invalid render target
    RenderTarget() : m_type(Type::Invalid), m_swapChain(nullptr) {}

    // Factory methods
    static RenderTarget createSwapChainTarget(SwapChain* swapChain);
    static RenderTarget createTextureTarget(const SmartTextureHandle& textureHandle);

    // Type queries
    bool isSwapchain() const { return m_type == Type::SwapChain; }
    bool isTexture() const { return m_type == Type::Texture; }
    bool isInvalid() const { return m_type == Type::Invalid; }
    Type getType() const { return m_type; }

    // Smart handle access
    const SmartTextureHandle& getTextureHandle() const;

    // SwapChain access
    SwapChain* getSwapChain() const;

    // Dimension extraction - no external parameters needed
    VkExtent2D getDimensions() const;
    Settings::MsaaSampleCount getSampleCount() const;

    // Validity check
    bool isValid() const;

    // Comparison operators for caching
    bool operator==(const RenderTarget& other) const;
    bool operator!=(const RenderTarget& other) const;

    // Hash function for use in unordered containers
    size_t hash() const;

private:
    // Private constructors to enforce factory methods
    explicit RenderTarget(SwapChain* swapChain);
    explicit RenderTarget(const SmartTextureHandle& textureHandle);

    Type m_type;
    SwapChain* m_swapChain = nullptr;           // Valid when m_type == Type::SwapChain
    SmartTextureHandle m_textureHandle;         // Valid when m_type == Type::Texture
};

// Hash specialization for std::unordered_map
namespace std {
    template<>
    struct hash<RenderTarget> {
        size_t operator()(const RenderTarget& target) const {
            return target.hash();
        }
    };
}