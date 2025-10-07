#pragma once
#include "RenderNodeTemplate.h"
#include "RenderTypes.h"
#include "RenderTarget.h"
#include <memory>

class EngineCore;
class ImGuiDrawCall;

/**
 * ImGui rendering template - specialized for UI overlay rendering.
 * Always renders to existing render pass with load operation.
 */
class ImGuiRenderNodeTemplate : public RenderNodeTemplate {
public:
    explicit ImGuiRenderNodeTemplate(EngineCore& engineCore);
    virtual ~ImGuiRenderNodeTemplate() = default;

    static constexpr const char* name = "ImGui";

    static constexpr RenderTemplateInfo getStaticTemplateInfo() {
        return RenderTemplateInfo(
            name,
            false,  // requiresDepthBuffer - ImGui doesn't need depth
            true,   // requiresColorBuffer
            true,   // supportsMSAA - can render to MSAA targets
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_UNDEFINED  // no depth format needed
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

    std::unordered_set<std::type_index> getAcceptedGpuCallTypes() const override {
        return makeTypeSet<ImGuiDrawCall>();
    }

    struct ImGuiConfig {
        bool enableMSAA = true;
        // ImGui always loads existing content (renders on top)
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    };

    void setConfig(const ImGuiConfig& config) { m_config = config; }
    const ImGuiConfig& getConfig() const { return m_config; }

private:
    ImGuiConfig m_config;

    RenderPassConfig createImGuiRenderPassConfig(
        const RenderParameters& params) const;

    void setupRenderNodeAttachments(
        class ImGuiRenderNode& node,
        const RenderParameters& params,
        const RenderPassConfig& passConfig) const;

    VkFormat selectColorFormat(const RenderTarget& target) const;
    VkSampleCountFlagBits selectSampleCount(const RenderTarget& target) const;
};

// Template type traits specialization
template<>
struct TemplateTypeTraits<ImGuiRenderNodeTemplate> {
    using template_type = ImGuiRenderNodeTemplate;
    static constexpr const char* name = ImGuiRenderNodeTemplate::name;

    static constexpr RenderTemplateInfo getStaticTemplateInfo() {
        return ImGuiRenderNodeTemplate::getStaticTemplateInfo();
    }

    static constexpr size_t type_hash() {
        return detail::fnv1a_hash(name);
    }
};