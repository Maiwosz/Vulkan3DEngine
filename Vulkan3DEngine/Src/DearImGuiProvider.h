#pragma once
#include "IImGuiProvider.h"
#include "VulkanContext.h"
#include "Window.h"
#include "SwapChain.h"
#include <unordered_map>

/**
 * Dear ImGui implementation of IImGuiProvider.
 * Manages Dear ImGui lifecycle and integrates with Vulkan render graph.
 */
class DearImGuiProvider : public IImGuiProvider {
public:
    DearImGuiProvider(Window& window, VulkanContext& context, SwapChain& swapchain);
    ~DearImGuiProvider() override;

    // Disable copy and move
    DearImGuiProvider(const DearImGuiProvider&) = delete;
    DearImGuiProvider& operator=(const DearImGuiProvider&) = delete;
    DearImGuiProvider(DearImGuiProvider&&) = delete;
    DearImGuiProvider& operator=(DearImGuiProvider&&) = delete;

    // IImGuiProvider interface
    void beginFrame() override;
    void endFrame() override;
    void render(VkCommandBuffer commandBuffer) override;
    bool initialize(SmartRenderPassHandle renderPass, VkSampleCountFlagBits msaaSamples) override;
    void shutdown() override;
    bool isInitialized() const override { return m_initialized; }
    SmartRenderPassHandle getCurrentRenderPass() const override { return m_currentRenderPass; }

    uint32_t registerCallback(ImGuiCallback callback) override;
    bool unregisterCallback(uint32_t callbackId) override;
    void clearCallbacks() override;
    void executeCallbacks() override;
    size_t getCallbackCount() const override { return m_callbacks.size(); }

private:
    Window& m_window;
    VulkanContext& m_context;
    SwapChain& m_swapchain;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    SmartRenderPassHandle m_currentRenderPass;
    VkSampleCountFlagBits m_currentMSAASamples = VK_SAMPLE_COUNT_1_BIT;
    
    bool m_globalInitialized = false;  // GLFW and ImGui context
    bool m_initialized = false;        // Vulkan backend for specific render pass

    uint32_t m_nextCallbackId = 0;
    std::unordered_map<uint32_t, ImGuiCallback> m_callbacks;

    void initializeGlobal();
    void shutdownGlobal();
    void createDescriptorPool();
    void destroyDescriptorPool();
    
    bool initializeVulkanBackend();
    void shutdownVulkanBackend();
};