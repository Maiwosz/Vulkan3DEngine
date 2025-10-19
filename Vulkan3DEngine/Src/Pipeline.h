#pragma once
#include <vulkan/vulkan.h>
#include "LogicalDevice.h"

enum class PipelineType {
    Graphics,
    Compute
};

class Pipeline {
public:
    Pipeline(
        const LogicalDevice& device,
        VkPipeline pipeline,
        VkPipelineLayout layout,
        PipelineType type
    );
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    VkPipeline get() const { return m_pipeline; }
    VkPipelineLayout getLayout() const { return m_pipelineLayout; }
    PipelineType getType() const { return m_type; }

    // Zwraca odpowiedni bind point dla typu pipeline
    VkPipelineBindPoint getBindPoint() const {
        return m_type == PipelineType::Graphics
            ? VK_PIPELINE_BIND_POINT_GRAPHICS
            : VK_PIPELINE_BIND_POINT_COMPUTE;
    }

private:
    const LogicalDevice& m_device;
    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;
    PipelineType m_type;
};