#pragma once
#include "Prerequisites.h"
#include "Window.h"
#include "VulkanContext.h"

class ImGuiWrapper {
public:
    ImGuiWrapper(
        Window& window,
        VulkanContext& context,
        VkRenderPass renderPass,
        uint32_t minImageCount,
        uint32_t imageCount,
        VkSampleCountFlagBits msaaSamples
    );
    ~ImGuiWrapper();

    void recreate();
    void render(VkCommandBuffer commandBuffer);

private:
    Window& r_window;
    VulkanContext& r_context;
    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
    bool m_initialized = false;

    VkRenderPass m_renderPass;
    uint32_t m_minImageCount;
    uint32_t m_imageCount;
    VkSampleCountFlagBits m_msaaSamples;

    void init();
    void shutdown();
    void createDescriptorPool();
};