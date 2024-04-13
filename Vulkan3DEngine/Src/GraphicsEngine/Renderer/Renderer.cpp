#include "Renderer.h"
#include "Buffers/StagingBuffer/StagingBuffer.h"
#include "Buffers/VertexBuffer/VertexBuffer.h"
#include "Buffers/IndexBuffer/IndexBuffer.h"
#include "Buffers/UniformBuffer/UniformBuffer.h"
#include "Image/Image.h"
#include "ImageView/ImageView.h"
#include "TextureSampler/TextureSampler.h"
#include "DescriptorSets/DescriptorSet/GlobalDescriptorSet/GlobalDescriptorSet.h"
#include "DescriptorSets/DescriptorSet/TextureDescriptorSet/TextureDescriptorSet.h"
#include "DescriptorSets/DescriptorSet/TransformDescriptorSet/TransformDescriptorSet.h"
#include "DescriptorSets/DescriptorPool/DescriptorPool.h"

#include "../../Application/Application.h"

Renderer::Renderer(WindowPtr window) : m_window(window)
{
    try {
        m_device = std::make_shared<Device>(this);
    }
    catch (...) { throw std::exception("Device not created successfully"); }

    try {
        m_swapChain = std::make_shared<SwapChain>(this);
    }
    catch (...) { throw std::exception("SwapChain not created successfully"); }

    try {
        m_globalDescriptorSetLayout = std::make_shared<GlobalDescriptorSetLayout>(this);
    }
    catch (...) { throw std::exception("GlobalDescriptorSetLayout not created successfully"); }

    try {
        m_textureDescriptorSetLayout = std::make_shared<TextureDescriptorSetLayout>(this);
    }
    catch (...) { throw std::exception("TextureDescriptorSetLayout not created successfully"); }

    try {
        m_transformDescriptorSetLayout = std::make_shared<TransformDescriptorSetLayout>(this);
    }
    catch (...) { throw std::exception("TransformDescriptorSetLayout not created successfully"); }

    try {
        m_graphicsPipeline = std::make_shared<GraphicsPipeline>(this);
    }
    catch (...) { throw std::exception("GraphicsPipeline not created successfully"); }
    
    createCommandBuffers();

    try {
        m_textureSampler = std::make_shared<TextureSampler>(this);
    }
    catch (...) { throw std::exception("TextureSampler not created successfully"); }

    try {
        m_descriptorPool = std::make_shared<DescriptorPool>(64, this);
    }
    catch (...) { throw std::exception("DescriptorPool not created successfully"); }

    
}

Renderer::~Renderer()
{
}

StagingBufferPtr Renderer::createStagingBuffer(VkDeviceSize bufferSize)
{
    return std::make_shared<StagingBuffer>(bufferSize, this);
}

VertexBufferPtr Renderer::createVertexBuffer(std::vector<Vertex> vertices)
{
    return std::make_shared<VertexBuffer>(vertices, this);
}

IndexBufferPtr Renderer::createIndexBuffer(std::vector<uint32_t> indices)
{
    return std::make_shared<IndexBuffer>(indices, this);
}

UniformBufferPtr Renderer::createUniformBuffer(VkDeviceSize bufferSize)
{
    return std::make_shared<UniformBuffer>(bufferSize, this);
}

ImagePtr Renderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
    return std::make_shared<Image>(width, height, format, tiling, usage, properties, this);
}

ImageViewPtr Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    return std::make_shared<ImageView>(image, format, aspectFlags, this);
}

TextureSamplerPtr Renderer::createTextureSampler()
{
    return std::make_shared<TextureSampler>(this);
}

TextureDescriptorSetPtr Renderer::createTextureDescriptorSet(VkImageView imageView, VkSampler sampler)
{
    return std::make_shared<TextureDescriptorSet>(imageView, sampler, this);
}

TransformDescriptorSetPtr Renderer::createTransformDescriptorSet(VkBuffer uniformBuffer)
{
    return std::make_shared<TransformDescriptorSet>(uniformBuffer, this);
}

GlobalDescriptorSetPtr Renderer::createGlobalDescriptorSet(VkBuffer uniformBuffer)
{
    return std::make_shared<GlobalDescriptorSet>(uniformBuffer, this);
}

void Renderer::drawFrameBegin()
{
    vkWaitForFences(m_device->get(), 1, &m_swapChain->m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    
    VkResult result = vkAcquireNextImageKHR(m_device->get(), m_swapChain->m_swapChain, UINT64_MAX,
        m_swapChain->m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
         m_swapChain->recreateSwapChain();
         return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
         throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(m_device->get(), 1, &m_swapChain->m_inFlightFences[m_currentFrame]);

    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    recordCommandBufferBegin(m_commandBuffers[m_currentFrame], m_currentImageIndex);
}

void Renderer::drawFrameEnd()
{
    recordCommandBufferEnd(m_commandBuffers[m_currentFrame]);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { m_swapChain->m_imageAvailableSemaphores[m_currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];

    VkSemaphore signalSemaphores[] = { m_swapChain->m_renderFinishedSemaphores[m_currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_device->getGraphicsQueue(), 1, &submitInfo, m_swapChain->m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { m_swapChain->m_swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &m_currentImageIndex;
    presentInfo.pResults = nullptr; // Optional

    VkResult result = vkQueuePresentKHR(m_device->getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window->wasWindowResized()) {
        m_window->resetWindowResizedFlag();
        m_swapChain->recreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::bindDescriptorSets()
{
    vkCmdBindDescriptorSets(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline->getLayout(),
        0, static_cast<uint32_t>(sizeof(m_currentDescriptorSets) / sizeof(m_currentDescriptorSets[0])), m_currentDescriptorSets, 0, nullptr);
}

void Renderer::createCommandBuffers()
{
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool =  m_device->getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

    if (vkAllocateCommandBuffers(m_device->get(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void Renderer::recordCommandBufferBegin(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_swapChain->getRenderPass();
    renderPassInfo.framebuffer = m_swapChain->getSwapChainFramebuffers()[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_swapChain->getSwapChainExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };
    
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline->get());

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_swapChain->getSwapChainExtent().width;
    viewport.height = (float)m_swapChain->getSwapChainExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_swapChain->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void Renderer::recordCommandBufferEnd(VkCommandBuffer commandBuffer)
{
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}
