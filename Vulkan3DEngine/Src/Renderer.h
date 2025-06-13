#pragma once
#include "Prerequisites.h"
#include "Window.h"
#include "SwapChain.h"
#include "ImGuiWrapper.h"
#include "ShaderModuleManager.h"
#include "CommandBuffer.h"
#include "FrameManager.h"
#include "RenderPassManager.h"
#include "VulkanContext.h"
#include "AttachmentManager.h"
#include "FrameBufferManager.h"
#include "Event.h"
#include "PipelineManager.h"
#include "UniformBufferManager.h"
#include "PipelineLayoutManager.h"
#include "DescriptorLayoutManager.h"
#include "ImageSamplerManager.h"
#include "DescriptorAllocator.h"
#include "Settings.h"

class Renderer
{
public:
    Renderer(Settings& settings, Window& window);
    ~Renderer();

	void advanceFrame();

    void waitIdle() { vkDeviceWaitIdle(m_vulkanContext->logical().get()); }

    VulkanContext& vulkanContext() { return *m_vulkanContext; }
    ShaderModuleManager& shaderModuleManager() { return *m_shaderModuleManager; }
    CommandBufferManager& commandBufferManager() { return *m_commandBufferManager; }
    SynchronizationResourceManager& synchronizationResourceManager() { return *m_syncResourceManager; }
    FrameManager& frameManager() { return *m_frameManager; }
    VramManager& vramManager() { return *m_vramManager; }
    UniformBufferManager& uniformBufferManager() { return *m_uniformBufferManager; }
    DescriptorLayoutManager& descriptorLayoutManager() { return *m_descriptorLayoutManager; }
    PipelineLayoutManager& pipelineLayoutManager() { return *m_pipelineLayoutManager; }
    SwapChain& swapChain() { return *m_swapChain; }
    ImageSamplerManager& imageSamplerManager() { return *m_samplerManager; }
    AttachmentManager& attachmentManager() { return *m_attachmentManager; }
    RenderPassManager& renderPassManager() { return *m_renderPassManager; }
    FrameBufferManager& framebufferManager() { return *m_framebufferManager; }
    PipelineManager& pipelineManager() { return *m_pipelineManager; }
    DescriptorAllocator& descriptorAllocator() { return *m_descriptorAllocator; }

    //RenderPassHandle renderPass() { return m_mainRenderPassHandle; }
    RenderPassHandle renderPass() {
        SPDLOG_INFO("renderPass() called, m_mainRenderPassHandle.id = {}", m_mainRenderPassHandle.id);
        RenderPassHandle result = m_mainRenderPassHandle;
        SPDLOG_INFO("renderPass() returning, result.id = {}", result.id);
        return result;
    }
    AttachmentHandle depthAttachmentHandle() { return m_depthAttachmentHandle; }
    AttachmentHandle msColorAttachmentHandle() { return m_msColorAttachmentHandle; }

    void recreateSwapChain();
    void recreateRenderPass();
    void recreateAttachments();

private:
	Settings& m_settings;
    Window& m_window;
    std::unique_ptr<VulkanContext> m_vulkanContext;
    std::unique_ptr<ShaderModuleManager> m_shaderModuleManager;
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
    std::unique_ptr<PipelineManager> m_pipelineManager;
    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator;

    // Main render pass and resources
    RenderPassHandle m_mainRenderPassHandle;
    AttachmentHandle m_depthAttachmentHandle;
    AttachmentHandle m_msColorAttachmentHandle;

    // Helper methods
    void createMainRenderPass();
};