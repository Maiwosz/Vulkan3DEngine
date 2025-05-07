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
#include "ImageSamplerManager.h"
#include "DescriptorAllocator.h"
#include "MeshManager.h"

class Renderer
{
public:
    Renderer(Window& window);
    ~Renderer();

    void waitIdle() { vkDeviceWaitIdle(m_vulkanContext->logical().get()); }
    
    VulkanContext& vulkanContext() { return *m_vulkanContext; }
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
    ShaderModuleManager& shaderModuleManager() { return *m_shaderModuleManager; }
    PipelineManager& pipelineManager() { return *m_pipelineManager; }
    MaterialManager& materialManager() { return *m_materialManager; }
    DescriptorAllocator& descriptorAllocator() { return *m_descriptorAllocator; }
    MeshManager& meshManager() { return *m_meshManager; }

    RenderPassHandle renderPass() { return m_mainRenderPassHandle; }
    AttachmentHandle depthAttachmentHandle() { return m_depthAttachmentHandle; }

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
    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator;
    std::unique_ptr<MeshManager> m_meshManager;

    // Main render pass and resources
    RenderPassHandle m_mainRenderPassHandle;
    AttachmentHandle m_depthAttachmentHandle;

    // Helper methods
    void createMainRenderPass();
    void recreateRenderResources();
    void handleWindowResize(int width, int height);
};