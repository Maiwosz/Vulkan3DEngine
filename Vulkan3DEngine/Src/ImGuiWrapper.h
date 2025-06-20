#pragma once
#include "Prerequisites.h"
#include "Window.h"
#include "VulkanContext.h"
#include "RenderPassManager.h"

class ImGuiWrapper {
public:
    ImGuiWrapper(
        Window& window,
        VulkanContext& context,
        RenderPassHandle renderPassHandle,
        RenderPassManager& renderPassManager,
        uint32_t minImageCount,
        uint32_t imageCount,
        VkSampleCountFlagBits msaaSamples
    );
    ~ImGuiWrapper();

    void recreate();
    void render(VkCommandBuffer commandBuffer);

private:
    Window& m_window;
    VulkanContext& m_context;
    RenderPassHandle m_renderPassHandle;
    RenderPassManager& m_renderPassManager;
    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
    bool m_initialized = false;

    uint32_t m_minImageCount;
    uint32_t m_imageCount;
    VkSampleCountFlagBits m_msaaSamples;

    void init();
    void shutdown();
    void createDescriptorPool();

    // Helper method to get current render pass
    VkRenderPass getCurrentRenderPass() const;
};