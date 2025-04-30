#pragma once
#include <vulkan/vulkan.h>
#include "LogicalDevice.h"

class Pipeline {
public:
    Pipeline(
        const LogicalDevice& device,
        VkPipeline pipeline,
        VkPipelineLayout layout
    );
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    VkPipeline get() const { return m_pipeline; }
    VkPipelineLayout getLayout() const { return m_pipelineLayout; }

private:
    const LogicalDevice& m_device;
    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;
};