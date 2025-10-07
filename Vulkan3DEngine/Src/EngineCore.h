#pragma once
#include "Prerequisites.h"
#include "Window.h"
#include "SwapChain.h"
#include "ImGuiCore.h"
#include "ShaderModuleManager.h"
#include "CommandBuffer.h"
#include "FrameManager.h"
#include "RenderPassManager.h"
#include "RenderNodeManager.h"
#include "RenderGraphManager.h"
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
#include "Renderer.h"
#include "RenderTarget.h"
#include "ForwardRenderGraphTemplate.h"
#include <optional> 

class Renderer;

class EngineCore
{
public:
    EngineCore(Settings& settings, Window& window);
    ~EngineCore();

    void advanceFrame();
    void waitIdle() { vkDeviceWaitIdle(m_vulkanContext->logical().get()); }

    // getters
    VulkanContext& vulkanContext() { return *m_vulkanContext; }
    ShaderModuleManager& shaderModuleManager() { return *m_shaderModuleManager; }
    CommandBufferManager& commandBufferManager() { return *m_commandBufferManager; }
    SynchronizationResourceManager& synchronizationResourceManager() { return *m_syncResourceManager; }
    FrameManager& frameManager() { return *m_frameManager; }
    VramManager& vramManager() { return *m_vramManager; }
    UniformBufferManager& uniformBufferManager() { return *m_uniformBufferManager; }
    DescriptorLayoutManager& descriptorLayoutManager() { return *m_descriptorLayoutManager; }
    PipelineLayoutManager& pipelineLayoutManager() { return *m_pipelineLayoutManager; }
    AttachmentManager& attachmentManager() { return *m_attachmentManager; }
    SwapChain& swapChain() { return *m_swapChain; }
    ImageSamplerManager& imageSamplerManager() { return *m_samplerManager; }
    RenderPassManager& renderPassManager() { return *m_renderPassManager; }
    RenderNodeManager& renderNodeManager() { return *m_renderNodeManager; }
    RenderGraphManager& renderGraphManager() { return *m_renderGraphManager; }
    TextureManager& textureManager() { return *m_textureManager; }
    FrameBufferManager& framebufferManager() { return *m_framebufferManager; }
    PipelineManager& pipelineManager() { return *m_pipelineManager; }
    DescriptorAllocator& descriptorAllocator() { 
		DescriptorAllocator& alloc = *m_descriptorAllocator;
        return alloc;
    }
    ImGuiCore& imguiCore() { return *m_imguiCore; }
    Renderer& renderer() { return *m_renderer; }

    void recreateSwapChain();

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
    std::unique_ptr<AttachmentManager> m_attachmentManager;
    std::unique_ptr<SwapChain> m_swapChain;
    std::unique_ptr<ImageSamplerManager> m_samplerManager;
    std::unique_ptr<RenderPassManager> m_renderPassManager;
    std::unique_ptr<TextureManager> m_textureManager;
    std::unique_ptr<RenderNodeManager> m_renderNodeManager;
    std::unique_ptr<RenderGraphManager> m_renderGraphManager;
    std::unique_ptr<FrameBufferManager> m_framebufferManager;
    std::unique_ptr<PipelineManager> m_pipelineManager;
    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator;
    std::unique_ptr<ImGuiCore> m_imguiCore;
    std::unique_ptr<Renderer> m_renderer;

    // Helper methods
    void initializeRenderGraphSystem();
};