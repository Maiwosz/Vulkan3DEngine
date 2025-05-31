#include "PipelineManager.h"
#include <stdexcept>

PipelineManager::PipelineManager(const LogicalDevice& device, ShaderModuleManager& shaderManager, PipelineLayoutManager& layoutManager)
    : m_device(device), m_shaderManager(shaderManager), m_layoutManager(layoutManager) {
}

PipelineManager::~PipelineManager() {
    // Clean up all pipelines
    m_pipelines.clear();
}

PipelineHandle PipelineManager::createGraphicsPipeline(const GraphicsPipelineConfig& config) {
    // Check if we already have this pipeline configuration
    auto cacheIt = m_pipelineCache.find(config);
    if (cacheIt != m_pipelineCache.end()) {
        return cacheIt->second;
    }

    // Check if layout handle is valid
    if (!m_layoutManager.isValid(config.layoutHandle)) {
        throw std::runtime_error("Invalid pipeline layout handle");
    }

    // Get pipeline layout
    VkPipelineLayout pipelineLayout = m_layoutManager.get(config.layoutHandle);

    // Create new pipeline
    VkPipeline vkPipeline = createVkPipeline(config, pipelineLayout);

    // Create handle and store pipeline
    PipelineHandle handle(m_nextHandle++);
    m_pipelines[handle] = std::make_unique<Pipeline>(m_device, vkPipeline, pipelineLayout);
    m_pipelineCache[config] = handle;

    return handle;
}

Pipeline& PipelineManager::get(PipelineHandle handle) {
    if (!isValid(handle)) {
        throw std::runtime_error("Invalid pipeline handle");
    }

    return *m_pipelines[handle];
}

bool PipelineManager::isValid(PipelineHandle handle) const {
    return m_pipelines.find(handle) != m_pipelines.end();
}

VkPipeline PipelineManager::createVkPipeline(const GraphicsPipelineConfig& config, VkPipelineLayout layout) {
    // Check if shaders are valid
    if (!config.shaderStages.vertexShader ||
        !config.shaderStages.fragmentShader) {
        throw std::runtime_error("Invalid shader module handle");
    }

    // Prepare shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    // Vertex shader stage
    VkPipelineShaderStageCreateInfo vertexShaderStageInfo{};
    vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageInfo.module = m_shaderManager.getModule(config.shaderStages.vertexShader)->get();
    vertexShaderStageInfo.pName = config.shaderStages.vertexEntryPoint.c_str();
    shaderStages.push_back(vertexShaderStageInfo);

    // Fragment shader stage
    VkPipelineShaderStageCreateInfo fragmentShaderStageInfo{};
    fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageInfo.module = m_shaderManager.getModule(config.shaderStages.fragmentShader)->get();
    fragmentShaderStageInfo.pName = config.shaderStages.fragmentEntryPoint.c_str();
    shaderStages.push_back(fragmentShaderStageInfo);

    // Geometry shader stage (optional)
    if (config.shaderStages.geometryShader) {
        VkPipelineShaderStageCreateInfo geometryShaderStageInfo{};
        geometryShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        geometryShaderStageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        geometryShaderStageInfo.module = m_shaderManager.getModule(config.shaderStages.geometryShader)->get();
        geometryShaderStageInfo.pName = config.shaderStages.geometryEntryPoint.c_str();
        shaderStages.push_back(geometryShaderStageInfo);
    }

    // Tessellation control shader stage (optional)
    if (config.shaderStages.tessControlShader) {
        VkPipelineShaderStageCreateInfo tessControlShaderStageInfo{};
        tessControlShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        tessControlShaderStageInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        tessControlShaderStageInfo.module = m_shaderManager.getModule(config.shaderStages.tessControlShader)->get();
        tessControlShaderStageInfo.pName = config.shaderStages.tessControlEntryPoint.c_str();
        shaderStages.push_back(tessControlShaderStageInfo);
    }

    // Tessellation evaluation shader stage (optional)
    if (config.shaderStages.tessEvalShader) {
        VkPipelineShaderStageCreateInfo tessEvalShaderStageInfo{};
        tessEvalShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        tessEvalShaderStageInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        tessEvalShaderStageInfo.module = m_shaderManager.getModule(config.shaderStages.tessEvalShader)->get();
        tessEvalShaderStageInfo.pName = config.shaderStages.tessEvalEntryPoint.c_str();
        shaderStages.push_back(tessEvalShaderStageInfo);
    }

    // Compute shader stage (optional)s
    if (config.shaderStages.computeShader) {
        VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
        computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeShaderStageInfo.module = m_shaderManager.getModule(config.shaderStages.computeShader)->get();
        computeShaderStageInfo.pName = config.shaderStages.computeEntryPoint.c_str();
        shaderStages.push_back(computeShaderStageInfo);
    }

    // Vertex input state
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(config.vertexInput.vertexBindings.size());
    vertexInputInfo.pVertexBindingDescriptions = config.vertexInput.vertexBindings.empty() ? nullptr : config.vertexInput.vertexBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(config.vertexInput.vertexAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = config.vertexInput.vertexAttributes.empty() ? nullptr : config.vertexInput.vertexAttributes.data();

    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport state
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    // Dynamic viewport and scissor will be set later

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = config.rasterization.polygonMode;
    rasterizer.cullMode = config.rasterization.cullMode;
    rasterizer.frontFace = config.rasterization.frontFace;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.lineWidth = config.rasterization.lineWidth;

    // Multisample state
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = config.multisample.samples;
    multisampling.sampleShadingEnable = VK_FALSE;

    // Depth-stencil state
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = config.depthStencil.depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = config.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = config.depthStencil.depthCompareOp;
    depthStencil.stencilTestEnable = config.depthStencil.stencilTestEnable ? VK_TRUE : VK_FALSE;

    // Color blend state
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = config.colorBlend.blendEnable ? VK_TRUE : VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = config.colorBlend.srcColorBlendFactor;
    colorBlendAttachment.dstColorBlendFactor = config.colorBlend.dstColorBlendFactor;
    colorBlendAttachment.colorBlendOp = config.colorBlend.colorBlendOp;
    colorBlendAttachment.srcAlphaBlendFactor = config.colorBlend.srcAlphaBlendFactor;
    colorBlendAttachment.dstAlphaBlendFactor = config.colorBlend.dstAlphaBlendFactor;
    colorBlendAttachment.alphaBlendOp = config.colorBlend.alphaBlendOp;
    colorBlendAttachment.colorWriteMask = config.colorBlend.colorWriteMask;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic states
    std::vector<VkDynamicState> dynamicStates;
    if (config.viewport.dynamicViewport) dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    if (config.viewport.dynamicScissor) dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.empty() ? nullptr : dynamicStates.data();

    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = dynamicStates.empty() ? nullptr : &dynamicState;
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = config.renderPass.renderPass;
    pipelineInfo.subpass = config.renderPass.subpass;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(m_device.get(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    return pipeline;
}