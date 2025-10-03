#pragma once
#include "RenderGraphTemplate.h"
#include "RenderGraph.h"
#include "RenderTypes.h"
#include "RenderTarget.h"
#include <memory>

class EngineCore;
class ForwardRenderNodeTemplate;

/**
 * Forward rendering graph template - simple single-node forward rendering pipeline.
 */
class ForwardRenderGraphTemplate : public RenderGraphTemplate {
public:
    explicit ForwardRenderGraphTemplate(EngineCore& engineCore);
    virtual ~ForwardRenderGraphTemplate() = default;

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
    };

    void setConfig(const ForwardGraphConfig& config) { m_config = config; }
    const ForwardGraphConfig& getConfig() const { return m_config; }

    static constexpr RenderGraphTemplateInfo getStaticTemplateInfo() {
        return RenderGraphTemplateInfo("Forward Rendering");
    }

    static constexpr const char* name = "Forward Rendering";

    static constexpr size_t type_hash() {
        constexpr const char* str = "ForwardRenderGraphTemplate";
        constexpr size_t basis = 14695981039346656037ULL;
        constexpr size_t prime = 1099511628211ULL;
        size_t hash = basis;
        for (const char* p = str; *p != '\0'; ++p) {
            hash ^= static_cast<size_t>(*p);
            hash *= prime;
        }
        return hash;
    }

private:
    ForwardGraphConfig m_config;
    bool validateTargetCompatibility(const RenderTarget& target) const;
};

template<>
struct GraphTemplateTypeTraits<ForwardRenderGraphTemplate> {
    static constexpr RenderGraphTemplateInfo getStaticTemplateInfo() {
        return ForwardRenderGraphTemplate::getStaticTemplateInfo();
    }
    static constexpr const char* name = ForwardRenderGraphTemplate::name;
    static constexpr size_t type_hash() {
        return ForwardRenderGraphTemplate::type_hash();
    }
};