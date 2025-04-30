#include "Pipeline.h"

Pipeline::Pipeline(
    const LogicalDevice& device,
    VkPipeline pipeline,
    VkPipelineLayout layout
) : m_device(device),
m_pipeline(pipeline),
m_pipelineLayout(layout)
{
}

Pipeline::~Pipeline() {
    vkDestroyPipeline(m_device.get(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device.get(), m_pipelineLayout, nullptr);
}