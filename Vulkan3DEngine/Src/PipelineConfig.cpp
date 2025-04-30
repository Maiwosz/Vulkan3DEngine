#include "PipelineConfig.h"
#include <cstring>

// ShaderStageConfig implementation
bool ShaderStageConfig::operator==(const ShaderStageConfig& other) const {
    return vertexShader == other.vertexShader &&
        fragmentShader == other.fragmentShader &&
        geometryShader == other.geometryShader &&
        computeShader == other.computeShader &&
        tessControlShader == other.tessControlShader &&
        tessEvalShader == other.tessEvalShader &&
        vertexEntryPoint == other.vertexEntryPoint &&
        fragmentEntryPoint == other.fragmentEntryPoint &&
        geometryEntryPoint == other.geometryEntryPoint &&
        computeEntryPoint == other.computeEntryPoint &&
        tessControlEntryPoint == other.tessControlEntryPoint &&
        tessEvalEntryPoint == other.tessEvalEntryPoint;
}

// PipelineLayoutConfig implementation
bool PipelineLayoutConfig::operator==(const PipelineLayoutConfig& other) const {
    if (pushConstantRanges.size() != other.pushConstantRanges.size() ||
        descriptorSetLayouts.size() != other.descriptorSetLayouts.size()) {
        return false;
    }

    for (size_t i = 0; i < pushConstantRanges.size(); ++i) {
        const auto& a = pushConstantRanges[i];
        const auto& b = other.pushConstantRanges[i];
        if (a.stageFlags != b.stageFlags || a.offset != b.offset || a.size != b.size) {
            return false;
        }
    }

    for (size_t i = 0; i < descriptorSetLayouts.size(); ++i) {
        if (descriptorSetLayouts[i] != other.descriptorSetLayouts[i]) {
            return false;
        }
    }

    return true;
}

// VertexInputConfig implementation
bool VertexInputConfig::operator==(const VertexInputConfig& other) const {
    if (vertexBindings.size() != other.vertexBindings.size() ||
        vertexAttributes.size() != other.vertexAttributes.size()) {
        return false;
    }

    for (size_t i = 0; i < vertexBindings.size(); ++i) {
        const auto& a = vertexBindings[i];
        const auto& b = other.vertexBindings[i];
        if (a.binding != b.binding || a.stride != b.stride || a.inputRate != b.inputRate) {
            return false;
        }
    }

    for (size_t i = 0; i < vertexAttributes.size(); ++i) {
        const auto& a = vertexAttributes[i];
        const auto& b = other.vertexAttributes[i];
        if (a.location != b.location || a.binding != b.binding ||
            a.format != b.format || a.offset != b.offset) {
            return false;
        }
    }

    return true;
}

// RasterizationConfig implementation
bool RasterizationConfig::operator==(const RasterizationConfig& other) const {
    return polygonMode == other.polygonMode &&
        cullMode == other.cullMode &&
        frontFace == other.frontFace &&
        lineWidth == other.lineWidth;
}

// DepthStencilConfig implementation
bool DepthStencilConfig::operator==(const DepthStencilConfig& other) const {
    return depthTestEnable == other.depthTestEnable &&
        depthWriteEnable == other.depthWriteEnable &&
        depthCompareOp == other.depthCompareOp &&
        stencilTestEnable == other.stencilTestEnable;
}

// ColorBlendConfig implementation
bool ColorBlendConfig::operator==(const ColorBlendConfig& other) const {
    return blendEnable == other.blendEnable &&
        srcColorBlendFactor == other.srcColorBlendFactor &&
        dstColorBlendFactor == other.dstColorBlendFactor &&
        colorBlendOp == other.colorBlendOp &&
        srcAlphaBlendFactor == other.srcAlphaBlendFactor &&
        dstAlphaBlendFactor == other.dstAlphaBlendFactor &&
        alphaBlendOp == other.alphaBlendOp &&
        colorWriteMask == other.colorWriteMask;
}

// ViewportConfig implementation
bool ViewportConfig::operator==(const ViewportConfig& other) const {
    return dynamicViewport == other.dynamicViewport &&
        dynamicScissor == other.dynamicScissor;
}

// MultisampleConfig implementation
bool MultisampleConfig::operator==(const MultisampleConfig& other) const {
    return samples == other.samples;
}

// PipelineRenderPassConfig implementation
bool PipelineRenderPassConfig::operator==(const PipelineRenderPassConfig& other) const {
    return renderPass == other.renderPass &&
        subpass == other.subpass;
}

// Hash function implementations
namespace std {

    size_t hash<ShaderStageConfig>::operator()(const ShaderStageConfig& config) const {
        size_t seed = 0;

        // Combine the hashes of shader handles
        hash_combine(seed, config.vertexShader.id);
        hash_combine(seed, config.fragmentShader.id);
        hash_combine(seed, config.geometryShader.id);
        hash_combine(seed, config.computeShader.id);
        hash_combine(seed, config.tessControlShader.id);
        hash_combine(seed, config.tessEvalShader.id);

        // Combine the hashes of entry points
        hash_combine(seed, hash<std::string>()(config.vertexEntryPoint));
        hash_combine(seed, hash<std::string>()(config.fragmentEntryPoint));
        hash_combine(seed, hash<std::string>()(config.geometryEntryPoint));
        hash_combine(seed, hash<std::string>()(config.computeEntryPoint));
        hash_combine(seed, hash<std::string>()(config.tessControlEntryPoint));
        hash_combine(seed, hash<std::string>()(config.tessEvalEntryPoint));

        return seed;
    }

    size_t hash<PipelineLayoutConfig>::operator()(const PipelineLayoutConfig& config) const {
        size_t seed = 0;

        // Hash push constant ranges
        for (const auto& range : config.pushConstantRanges) {
            hash_combine(seed, range.stageFlags);
            hash_combine(seed, range.offset);
            hash_combine(seed, range.size);
        }

        // Hash descriptor set layouts
        for (const auto& layout : config.descriptorSetLayouts) {
            hash_combine(seed, reinterpret_cast<size_t>(layout));
        }

        return seed;
    }

    size_t hash<VertexInputConfig>::operator()(const VertexInputConfig& config) const {
        size_t seed = 0;

        // Hash vertex bindings
        for (const auto& binding : config.vertexBindings) {
            hash_combine(seed, binding.binding);
            hash_combine(seed, binding.stride);
            hash_combine(seed, binding.inputRate);
        }

        // Hash vertex attributes
        for (const auto& attribute : config.vertexAttributes) {
            hash_combine(seed, attribute.location);
            hash_combine(seed, attribute.binding);
            hash_combine(seed, attribute.format);
            hash_combine(seed, attribute.offset);
        }

        return seed;
    }

    size_t hash<RasterizationConfig>::operator()(const RasterizationConfig& config) const {
        size_t seed = 0;

        hash_combine(seed, static_cast<int>(config.polygonMode));
        hash_combine(seed, static_cast<int>(config.cullMode));
        hash_combine(seed, static_cast<int>(config.frontFace));

        // Use a precise hash for the float value
        uint32_t lineWidthBits;
        memcpy(&lineWidthBits, &config.lineWidth, sizeof(float));
        hash_combine(seed, lineWidthBits);

        return seed;
    }

    size_t hash<DepthStencilConfig>::operator()(const DepthStencilConfig& config) const {
        size_t seed = 0;

        hash_combine(seed, config.depthTestEnable);
        hash_combine(seed, config.depthWriteEnable);
        hash_combine(seed, static_cast<int>(config.depthCompareOp));
        hash_combine(seed, config.stencilTestEnable);

        return seed;
    }

    size_t hash<ColorBlendConfig>::operator()(const ColorBlendConfig& config) const {
        size_t seed = 0;

        hash_combine(seed, config.blendEnable);
        hash_combine(seed, static_cast<int>(config.srcColorBlendFactor));
        hash_combine(seed, static_cast<int>(config.dstColorBlendFactor));
        hash_combine(seed, static_cast<int>(config.colorBlendOp));
        hash_combine(seed, static_cast<int>(config.srcAlphaBlendFactor));
        hash_combine(seed, static_cast<int>(config.dstAlphaBlendFactor));
        hash_combine(seed, static_cast<int>(config.alphaBlendOp));
        hash_combine(seed, static_cast<int>(config.colorWriteMask));

        return seed;
    }

    size_t hash<ViewportConfig>::operator()(const ViewportConfig& config) const {
        size_t seed = 0;

        hash_combine(seed, config.dynamicViewport);
        hash_combine(seed, config.dynamicScissor);

        return seed;
    }

    size_t hash<MultisampleConfig>::operator()(const MultisampleConfig& config) const {
        return std::hash<int>()(static_cast<int>(config.samples));
    }

    size_t hash<PipelineRenderPassConfig>::operator()(const PipelineRenderPassConfig& config) const {
        size_t seed = 0;

        hash_combine(seed, reinterpret_cast<size_t>(config.renderPass));
        hash_combine(seed, config.subpass);

        return seed;
    }

} // namespace std