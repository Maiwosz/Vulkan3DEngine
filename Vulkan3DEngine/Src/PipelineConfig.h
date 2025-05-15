#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <functional>
#include "Handle.h"

namespace std {
    // Helper function for combining hash values
    template <class T>
    inline void hash_combine(size_t& seed, const T& v) {
        seed ^= hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
}

struct ShaderStageConfig {
    ShaderModuleHandle vertexShader = ShaderModuleHandle(0);
    ShaderModuleHandle fragmentShader = ShaderModuleHandle(0);
    ShaderModuleHandle geometryShader = ShaderModuleHandle(0);
    ShaderModuleHandle computeShader = ShaderModuleHandle(0);
    ShaderModuleHandle tessControlShader = ShaderModuleHandle(0);
    ShaderModuleHandle tessEvalShader = ShaderModuleHandle(0);

    std::string vertexEntryPoint = "main";
    std::string fragmentEntryPoint = "main";
    std::string geometryEntryPoint = "main";
    std::string computeEntryPoint = "main";
    std::string tessControlEntryPoint = "main";
    std::string tessEvalEntryPoint = "main";

    bool operator==(const ShaderStageConfig& other) const;
};

// Structure containing pipeline layout information
struct PipelineLayoutConfig {
    std::vector<VkPushConstantRange> pushConstantRanges;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    bool operator==(const PipelineLayoutConfig& other) const;
};

// Structure containing vertex input information
struct VertexInputConfig {
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;

    bool operator==(const VertexInputConfig& other) const;
};

// Structure containing rasterization information
struct RasterizationConfig {
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    float lineWidth = 1.0f;

    bool operator==(const RasterizationConfig& other) const;
};

// Structure containing depth/stencil information
struct DepthStencilConfig {
    bool depthTestEnable = true;
    bool depthWriteEnable = true;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
    bool stencilTestEnable = false;
    // More stencil test parameters can be added here

    bool operator==(const DepthStencilConfig& other) const;
};

// Structure containing blending information
struct ColorBlendConfig {
    bool blendEnable = false;
    VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
    VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;
    VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    bool operator==(const ColorBlendConfig& other) const;
};

// Structure containing viewport state information
struct ViewportConfig {
    bool dynamicViewport = true;
    bool dynamicScissor = true;
    // Static viewports and scissors can be added when the above are false

    bool operator==(const ViewportConfig& other) const;
};

// Structure containing multisampling information
struct MultisampleConfig {
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    // More multisample parameters can be added

    bool operator==(const MultisampleConfig& other) const;
};

// Structure containing render pass information
struct PipelineRenderPassConfig {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;

    bool operator==(const PipelineRenderPassConfig& other) const;
};

// Main pipeline config structure, composed of smaller structures
struct GraphicsPipelineConfig {
    ShaderStageConfig shaderStages;
    PipelineLayoutHandle layoutHandle;
    VertexInputConfig vertexInput;
    ViewportConfig viewport;
    RasterizationConfig rasterization;
    MultisampleConfig multisample;
    DepthStencilConfig depthStencil;
    ColorBlendConfig colorBlend;
    PipelineRenderPassConfig renderPass;

    bool operator==(const GraphicsPipelineConfig& other) const {
        return shaderStages == other.shaderStages &&
            layoutHandle == other.layoutHandle &&
            vertexInput == other.vertexInput &&
            viewport == other.viewport &&
            rasterization == other.rasterization &&
            multisample == other.multisample &&
            depthStencil == other.depthStencil &&
            colorBlend == other.colorBlend &&
            renderPass == other.renderPass;
    }
};

// Hash function implementations for pipeline config structures
namespace std {
    template <>
    struct hash<ShaderStageConfig> {
        size_t operator()(const ShaderStageConfig& config) const;
    };

    template <>
    struct hash<PipelineLayoutConfig> {
        size_t operator()(const PipelineLayoutConfig& config) const;
    };

    template <>
    struct hash<VertexInputConfig> {
        size_t operator()(const VertexInputConfig& config) const;
    };

    template <>
    struct hash<RasterizationConfig> {
        size_t operator()(const RasterizationConfig& config) const;
    };

    template <>
    struct hash<DepthStencilConfig> {
        size_t operator()(const DepthStencilConfig& config) const;
    };

    template <>
    struct hash<ColorBlendConfig> {
        size_t operator()(const ColorBlendConfig& config) const;
    };

    template <>
    struct hash<ViewportConfig> {
        size_t operator()(const ViewportConfig& config) const;
    };

    template <>
    struct hash<MultisampleConfig> {
        size_t operator()(const MultisampleConfig& config) const;
    };

    template <>
    struct hash<PipelineRenderPassConfig> {
        size_t operator()(const PipelineRenderPassConfig& config) const;
    };

    template <>
    struct hash<GraphicsPipelineConfig> {
        size_t operator()(const GraphicsPipelineConfig& config) const {
            size_t seed = 0;

            // Combine hashes of all structures
            hash_combine(seed, config.shaderStages);
            hash_combine(seed, config.layoutHandle);
            hash_combine(seed, config.vertexInput);
            hash_combine(seed, config.viewport);
            hash_combine(seed, config.rasterization);
            hash_combine(seed, config.multisample);
            hash_combine(seed, config.depthStencil);
            hash_combine(seed, config.colorBlend);
            hash_combine(seed, config.renderPass);

            return seed;
        }
    };
}