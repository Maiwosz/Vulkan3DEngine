#pragma once
#include "Prerequisites.h"
#include <vector>
#include <fstream>
#include <shaderc/shaderc.hpp>

class PipelineBuilder {
public:
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

    VkPipelineInputAssemblyStateCreateInfo m_inputAssembly;
    VkPipelineRasterizationStateCreateInfo m_rasterizer;
    VkPipelineColorBlendAttachmentState m_colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo m_multisampling;
    VkPipelineLayout m_pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo m_depthStencil;
    VkPipelineRenderingCreateInfo m_renderInfo;
    VkFormat m_colorAttachmentformat;

    PipelineBuilder(Renderer* renderer): m_renderer(renderer) { clear(); }

    void clear();

    VkPipeline buildPipeline(VkPipelineLayout layout, VkDevice device);

    void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void setInputTopology(VkPrimitiveTopology topology);
    void setPolygonMode(VkPolygonMode mode);
    void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void setMultisamplingNone();
    void setMultisampling(VkSampleCountFlagBits rasterizationSamples, float minSampleShading);
    void disableBlending();
    void enableBlendingAdditive();
    void enableBlendingAlphablend();

    void setColorAttachmentFormat(VkFormat format);
    void setDepthFormat(VkFormat format);
    void disableDepthtest();
    void enableDepthtest(bool depthWriteEnable, VkCompareOp op);
private:
    Renderer* m_renderer = nullptr;
};

//Shaders
VkShaderModule createShaderModule(const std::vector<uint32_t>& code, VkDevice device);
VkShaderModule loadShaderModule(const std::string& filename, VkDevice device);
static std::vector<char> readFile(const std::string& filename);
VkShaderModule compileShader(const std::string& filename, shaderc_shader_kind kind, VkDevice device);