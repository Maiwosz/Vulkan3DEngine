#pragma once
#include "RenderGraphTemplate.h"
#include "RenderGraph.h"
#include "RenderTypes.h"
#include "RenderTarget.h"
#include <memory>

class EngineCore;
class ForwardRenderNodeTemplate;
class ImGuiRenderNodeTemplate;

/**
 * Forward rendering graph template - forward rendering with optional ImGui overlay.
 */
class ForwardRenderGraphTemplate : public RenderGraphTemplate {
public:
    explicit ForwardRenderGraphTemplate(EngineCore& engineCore);
    virtual ~ForwardRenderGraphTemplate() = default;

    // Template name - unique identifier
    static constexpr const char* name = "Forward Rendering";

    // Static template information
    static constexpr RenderGraphTemplateInfo getStaticTemplateInfo() {
        return RenderGraphTemplateInfo(name);
    }

    const RenderGraphTemplateInfo& getTemplateInfo() const override;
    bool isCompatibleWithTarget(const RenderTarget& target) const override;

    std::unique_ptr<RenderGraph> createRenderGraph(
        const RenderTarget& target,
        VkExtent2D extent) const override;

    // Configuration
    struct ForwardGraphConfig {
        bool enableMSAA = true;
        bool enableDepthTesting = true;
        bool optimizeForTextures = false;
        bool enableImGui = false;  // NEW: Enable ImGui overlay
    };

    void setConfig(const ForwardGraphConfig& config) { m_config = config; }
    const ForwardGraphConfig& getConfig() const { return m_config; }

private:
    ForwardGraphConfig m_config;
    bool validateTargetCompatibility(const RenderTarget& target) const;
};

// Graph template type traits specialization
template<>
struct GraphTemplateTypeTraits<ForwardRenderGraphTemplate> {
    static constexpr const char* name = ForwardRenderGraphTemplate::name;

    static constexpr RenderGraphTemplateInfo getStaticTemplateInfo() {
        return ForwardRenderGraphTemplate::getStaticTemplateInfo();
    }

    // Universal type_hash based on name
    static constexpr size_t type_hash() {
        return detail::fnv1a_hash(name);
    }
};