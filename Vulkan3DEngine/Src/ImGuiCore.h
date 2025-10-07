#pragma once
#include "Prerequisites.h"
#include "Window.h"
#include "VulkanContext.h"
#include "RenderPassManager.h"
#include "SwapChain.h"
#include <memory>

// Forward declaration
class ImGuiRenderPass;

class ImGuiCore {
public:
    ImGuiCore(Window& window, VulkanContext& context, SwapChain& swapchain);
    ~ImGuiCore();

    // Disable copy and move
    ImGuiCore(const ImGuiCore&) = delete;
    ImGuiCore& operator=(const ImGuiCore&) = delete;
    ImGuiCore(ImGuiCore&&) = delete;
    ImGuiCore& operator=(ImGuiCore&&) = delete;

    // Factory method for creating render pass instances
    std::unique_ptr<ImGuiRenderPass> createRenderPass(
        SmartRenderPassHandle renderPass,
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT
    );

    // Call once per frame before any ImGui rendering
    void newFrame();

private:
    Window& m_window;
    VulkanContext& m_context;
    SwapChain& m_swapchain;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    bool m_initialized = false;

    void initGlobalImGui();
    void shutdownGlobalImGui();
    void createDescriptorPool();
};