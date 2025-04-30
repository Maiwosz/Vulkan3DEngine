#pragma once
#include "Prerequisites.h"
#include "Window.h"
#include "SwapChain.h"
#include "ImGuiWrapper.h"
#include "CommandBuffer.h"
#include "FrameManager.h"
#include "RenderPassManager.h"
#include "VulkanContext.h"
#include "AttachmentManager.h"
#include "FrameBufferManager.h"
#include "Event.h"
#include "ShaderModuleManager.h"
#include "PipelineManager.h"
#include "MaterialManager.h"
#include "UniformBufferManager.h"
#include "PipelineLayoutManager.h"
#include "DescriptorLayoutManager.h"

class Renderer
{
public:
    Renderer(Window& window);
    ~Renderer();

    void waitIdle() { vkDeviceWaitIdle(m_vulkanContext->logical().get()); }
    VramManager& getVramManager() { return *m_vramManager; }
    ShaderModuleManager& getShaderModuleManager() { return *m_shaderModuleManager; }
    MaterialManager& getMaterialManager() { return *m_materialManager; }

    // New render frame function
    void drawFrame();

private:
    Window& m_window;
    std::unique_ptr<VulkanContext> m_vulkanContext;
    std::unique_ptr<CommandBufferManager> m_commandBufferManager;
    std::unique_ptr<SynchronizationResourceManager> m_syncResourceManager;
    std::unique_ptr<FrameManager> m_frameManager;
    std::unique_ptr<VramManager> m_vramManager;
    std::unique_ptr<UniformBufferManager> m_uniformBufferManager;
    std::unique_ptr<DescriptorLayoutManager> m_descriptorLayoutManager;
    std::unique_ptr<PipelineLayoutManager> m_pipelineLayoutManager;
    std::unique_ptr<SwapChain> m_swapChain;
    std::unique_ptr<ImageSamplerManager> m_samplerManager;
    std::unique_ptr<AttachmentManager> m_attachmentManager;
    std::unique_ptr<RenderPassManager> m_renderPassManager;
    std::unique_ptr<FrameBufferManager> m_framebufferManager;
    std::unique_ptr<ShaderModuleManager> m_shaderModuleManager;
    std::unique_ptr<PipelineManager> m_pipelineManager;
    std::unique_ptr<MaterialManager> m_materialManager;

    // Main render pass and resources
    RenderPassHandle m_mainRenderPassHandle;
    AttachmentHandle m_depthAttachmentHandle;

    // Event subscriptions
    std::unique_ptr<Event<int, int>::Subscription> m_windowResizeSubscription;
    std::unique_ptr<Event<>::Subscription> m_swapChainRecreationSubscription;

    // Helper methods
    void createMainRenderPass();
    void recreateRenderResources();
    void handleWindowResize(int width, int height);
};