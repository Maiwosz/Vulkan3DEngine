#pragma once
#include "RenderTypes.h"
#include "RenderTarget.h"
#include "Handle.h"
#include <memory>
#include <typeindex>
#include "RenderPassManager.h"

// Forward declarations
class RenderNode;
class EngineCore;
class SwapChain;

// Smart handle type for render pass
using SmartRenderPassHandle = SmartHandle<RenderPassHandle, VkRenderPass>;

/**
 * Abstract factory for creating render nodes with access to engine systems.
 *
 * Architecture Role:
 * - Static metadata holder (via getTemplateInfo())
 * - Factory for creating node instances with proper Vulkan resources
 * - Has access to EngineCore for retrieving necessary managers/systems
 * - Bridge between "what to create" and "how to create it"
 */
class RenderNodeTemplate {
public:
    explicit RenderNodeTemplate(EngineCore& engineCore)
        : m_engineCore(engineCore) {
    }

    virtual ~RenderNodeTemplate() = default;

    // Static template metadata
    virtual const RenderTemplateInfo& getTemplateInfo() const = 0;

    // Type information
    virtual std::type_index getTypeIndex() const {
        return std::type_index(typeid(*this));
    }

    const char* getTemplateName() const { return getTemplateInfo().name; }

    // Capability queries
    virtual bool isCompatibleWithTarget(const RenderTarget& target) const = 0;

    // Render parameter specification
    struct RenderParameters {
        VkExtent2D extent;
        VkFormat colorFormat;
        VkFormat depthFormat;
        VkSampleCountFlagBits samples;
        bool hasDepth;
        bool hasColor;
        bool hasResolve;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    };

    // Simplified interface - uses EngineCore internally
    virtual RenderParameters queryRenderParameters(
        const RenderTarget& target,
        VkExtent2D extent) const = 0;

    // Factory method - simplified interface
    virtual std::unique_ptr<RenderNode> createRenderNode(
        const RenderTarget& target,
        VkExtent2D extent) const = 0;

protected:
    EngineCore& m_engineCore;

    // Helper to get SwapChain when needed
    SwapChain* getSwapChain() const;

    // Helper to create smart render pass handles
    SmartRenderPassHandle createSmartRenderPassHandle(
        const RenderPassConfig& config) const;

    // Helper to extract texture format
    VkFormat getTextureFormat(const RenderTarget& target) const;

    // Helper to get texture usage flags
    VkImageUsageFlags getTextureUsageFlags(const RenderTarget& target) const;

    // Helper to check multisampling support
    bool supportsMultisampling(const RenderTarget& target) const;
};