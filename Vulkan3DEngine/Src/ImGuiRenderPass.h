#pragma once
#include "Prerequisites.h"
#include "RenderPassManager.h"
#include "VulkanContext.h"

// Forward declaration
class ImGuiCore;

class ImGuiRenderPass {
public:
    ImGuiRenderPass(
        ImGuiCore& imguiContext,
        const VulkanContext& context,
        VkDescriptorPool pool,
        SmartRenderPassHandle renderPass,
        uint32_t imageCount,
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT
    );
    ~ImGuiRenderPass();

    // Disable copy and move
    ImGuiRenderPass(const ImGuiRenderPass&) = delete;
    ImGuiRenderPass& operator=(const ImGuiRenderPass&) = delete;
    ImGuiRenderPass(ImGuiRenderPass&&) = delete;
    ImGuiRenderPass& operator=(ImGuiRenderPass&&) = delete;

    // Recreate when render pass changes
    void recreate();

    // Render ImGui draw data to command buffer
    void render(VkCommandBuffer commandBuffer);

    // Check if initialized
    bool isInitialized() const { return m_initialized; }

private:
    ImGuiCore& m_imguiContext;
    const VulkanContext& m_context;
    VkDescriptorPool m_descriptorPool;
    SmartRenderPassHandle m_renderPass;
    uint32_t m_imageCount;
    VkSampleCountFlagBits m_msaaSamples;
    bool m_initialized = false;

    void init();
    void shutdown();
    VkRenderPass getCurrentRenderPass() const;
};