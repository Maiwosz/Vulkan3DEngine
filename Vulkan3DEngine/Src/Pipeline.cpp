#include "Pipeline.h"

// Zaktualizowany konstruktor z typem pipeline
Pipeline::Pipeline(
    const LogicalDevice& device,
    VkPipeline pipeline,
    VkPipelineLayout layout,
    PipelineType type
) : m_device(device),
m_pipeline(pipeline),
m_pipelineLayout(layout),
m_type(type)
{
}

// Destruktor pozostaje bez zmian
Pipeline::~Pipeline() {
    vkDestroyPipeline(m_device.get(), m_pipeline, nullptr);
}