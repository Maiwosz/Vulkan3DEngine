#pragma once
#include "RenderNodeTemplate.h"
#include "RenderTypes.h"
#include "RenderTarget.h"
#include <memory>

class EngineCore;
class DrawCall;
struct RenderPassConfig;

/**
 * Forward rendering template - simple single-pass forward renderer.
 */
class ForwardRenderNodeTemplate : public RenderNodeTemplate {
public:
    explicit ForwardRenderNodeTemplate(EngineCore& engineCore);
    virtual ~ForwardRenderNodeTemplate() = default;

    // Template name - unique identifier
    static constexpr const char* name = "Forward";

    // Static template information
    static constexpr RenderTemplateInfo getStaticTemplateInfo() {
        return RenderTemplateInfo(
            name,
            true,  // requiresDepthBuffer
            true,  // requiresColorBuffer
            true,  // supportsMSAA
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_D32_SFLOAT
        );
    }

    const RenderTemplateInfo& getTemplateInfo() const override {
        return getStaticTemplateInfo();
    }

    bool isCompatibleWithTarget(const RenderTarget& target) const override;

    RenderParameters queryRenderParameters(
        const RenderTarget& target,
        VkExtent2D extent) const override;

    std::unique_ptr<RenderNode> createRenderNode(
        const RenderTarget& target,
        VkExtent2D extent) const override;

    // Configuration
    struct ForwardConfig {
        bool enableMSAA = true;
        bool enableDepthTesting = true;
        bool optimizeForTextures = false;
    };

    void setConfig(const ForwardConfig& config) { m_config = config; }
    const ForwardConfig& getConfig() const { return m_config; }

    std::unordered_set<std::type_index> getAcceptedGpuCallTypes() const override {
        return makeTypeSet<DrawCall>(); // Only accepts DrawCalls
    }
private:
    ForwardConfig m_config;

    RenderPassConfig createForwardRenderPassConfig(
        const RenderParameters& params) const;

    void setupRenderNodeAttachments(
        RenderNode& node,
        const RenderParameters& params,
        const RenderPassConfig& passConfig) const;

    VkFormat selectColorFormat(const RenderTarget& target) const;
    VkFormat selectDepthFormat() const;
    VkSampleCountFlagBits selectSampleCount(const RenderTarget& target) const;
    VkImageLayout selectInitialColorLayout(const RenderTarget& target) const;
    VkImageLayout selectFinalColorLayout(const RenderTarget& target) const;
};

// Template type traits specialization
template<>
struct TemplateTypeTraits<ForwardRenderNodeTemplate> {
    using template_type = ForwardRenderNodeTemplate;
    static constexpr const char* name = ForwardRenderNodeTemplate::name;

    static constexpr RenderTemplateInfo getStaticTemplateInfo() {
        return ForwardRenderNodeTemplate::getStaticTemplateInfo();
    }

    // Universal type_hash based on name
    static constexpr size_t type_hash() {
        return detail::fnv1a_hash(name);
    }
};