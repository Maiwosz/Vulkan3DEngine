#include "Renderer.h"
#include "Buffer/VertexBuffer/VertexBuffer.h"
#include "Buffer/IndexBuffer/IndexBuffer.h"
#include "Buffer/UniformBuffer/UniformBuffer.h"

//updateUniformBuffer()
#define GLM_FORCE_RADIANS
//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
        m_descriptorSetLayout = std::make_shared<DescriptorSetLayout>(this);
    }
    catch (...) { throw std::exception("DescriptorSetLayout not created successfully"); }
    try {
        m_graphicsPipeline = std::make_shared<GraphicsPipeline>(this);
    }
    catch (...) { throw std::exception("GraphicsPipeline not created successfully"); }
    
    createCommandBuffers();

    try {
        m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            m_uniformBuffers[i] = std::make_shared<UniformBuffer>(this);
        }
    }
    catch (...) { throw std::exception("UniformBuffers not created successfully"); }

    createDescriptorPool();
    createDescriptorSets();
}

Renderer::~Renderer()
{
    vkDestroyDescriptorPool(m_device->get(), m_descriptorPool, nullptr);
}

VertexBufferPtr Renderer::createVertexBuffer(std::vector<Vertex> vertices)
{
    return std::make_shared<VertexBuffer>(vertices, this);
}

IndexBufferPtr Renderer::createIndexBuffer(std::vector<uint16_t> indices)
{
    return std::make_shared<IndexBuffer>(indices, this);
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

    //VkSubmitInfo submitInfo{};
    //submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    //
    //VkSemaphore waitSemaphores[] = { m_swapChain->m_imageAvailableSemaphores[m_currentFrame] };
    //VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    //submitInfo.waitSemaphoreCount = 1;
    //submitInfo.pWaitSemaphores = waitSemaphores;
    //submitInfo.pWaitDstStageMask = waitStages;
    //
    //submitInfo.commandBufferCount = 1;
    //submitInfo.pCommandBuffers = &commandBuffers[m_currentFrame];
    //
    //VkSemaphore signalSemaphores[] = { m_swapChain->m_renderFinishedSemaphores[m_currentFrame] };
    //submitInfo.signalSemaphoreCount = 1;
    //submitInfo.pSignalSemaphores = signalSemaphores;
    //
    //if (vkQueueSubmit(m_device->getGraphicsQueue(), 1, &submitInfo, m_swapChain->m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
    //    throw std::runtime_error("failed to submit draw command buffer!");
    //}
    //
    //VkPresentInfoKHR presentInfo{};
    //presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    //
    //presentInfo.waitSemaphoreCount = 1;
    //presentInfo.pWaitSemaphores = signalSemaphores;
    //
    //VkSwapchainKHR swapChains[] = { m_swapChain->m_swapChain };
    //presentInfo.swapchainCount = 1;
    //presentInfo.pSwapchains = swapChains;
    //presentInfo.pImageIndices = &imageIndex;
    //presentInfo.pResults = nullptr; // Optional
    //
    //result = vkQueuePresentKHR(m_device->getPresentQueue(), &presentInfo);
    //
    //if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window->wasWindowResized()) {
    //    m_window->resetWindowResizedFlag();
    //    m_swapChain->recreateSwapChain();
    //}
    //else if (result != VK_SUCCESS) {
    //    throw std::runtime_error("failed to present swap chain image!");
    //}
    //
    //m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::drawFrameEnd()
{
    recordCommandBufferEnd(m_commandBuffers[m_currentFrame]);

    updateUniformBuffer(m_currentFrame);

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

    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

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

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline->getLayout(),
        0, 1, &m_descriptorSets[m_currentFrame], 0, nullptr);


    //vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    //
    //vkCmdEndRenderPass(commandBuffer);
    //
    //if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    //    throw std::runtime_error("failed to record command buffer!");
    //}
}

void Renderer::recordCommandBufferEnd(VkCommandBuffer commandBuffer)
{
    //vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    //vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void Renderer::createUniformBuffers()
{
    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        UniformBufferPtr ub = std::make_shared<UniformBuffer>(this);
    }
}

void Renderer::updateUniformBuffer(uint32_t currentImage)
{
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), Application::s_deltaTime * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), m_swapChain->getSwapChainExtent().width / (float)m_swapChain->getSwapChainExtent().height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    memcpy(m_uniformBuffers[currentImage]->getMappedMemory(), &ubo, sizeof(ubo));
}

void Renderer::createDescriptorPool()
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(m_device->get(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void Renderer::createDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout->get());
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_device->get(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i]->get();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr; // Optional
        descriptorWrite.pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(m_device->get(), 1, &descriptorWrite, 0, nullptr);
    }
}
