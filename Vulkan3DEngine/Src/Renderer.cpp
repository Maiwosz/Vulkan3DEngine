#include "Renderer.h"
#include "StagingBuffer.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "UniformBuffer.h"
#include "Image.h"
#include "ImageView.h"
#include "TextureSampler.h"
#include "GlobalDescriptorSet.h"
#include "TextureDescriptorSet.h"
#include "ModelDescriptorSet.h"
#include "DescriptorPool.h"

#include "Application.h"
#include "RendererInits.h"

VkSampleCountFlagBits Renderer::s_msaaSamples = VK_SAMPLE_COUNT_1_BIT;

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
        m_modelDescriptorSetLayout = std::make_shared<ModelDescriptorSetLayout>(this);
    }
    catch (...) { throw std::exception("ModelDescriptorSetLayout not created successfully"); }

    createGraphicsPipeline();
    
    createCommandBuffers();

    try {
        m_descriptorPool = std::make_shared<DescriptorPool>(64, this);
    }
    catch (...) { throw std::exception("DescriptorPool not created successfully"); }

    
}

Renderer::~Renderer()
{
    vkDestroyPipeline(m_device->get(), m_graphicsPipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(m_device->get(), m_graphicsPipeline.layout, nullptr);
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

ImagePtr Renderer::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
    return std::make_shared<Image>(width, height, mipLevels, numSamples, format, tiling, usage, properties, this);
}

ImageViewPtr Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
    return std::make_shared<ImageView>(image, format, aspectFlags, mipLevels, this);
}

TextureSamplerPtr Renderer::createTextureSampler(uint32_t mipLevels)
{
    return std::make_shared<TextureSampler>(mipLevels, this);
}

TextureDescriptorSetPtr Renderer::createTextureDescriptorSet(VkImageView imageView, VkSampler sampler)
{
    return std::make_shared<TextureDescriptorSet>(imageView, sampler, this);
}

ModelDescriptorSetPtr Renderer::createModelDescriptorSet(VkBuffer uniformBuffer)
{
    return std::make_shared<ModelDescriptorSet>(uniformBuffer, this);
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

    m_currentFrame = (m_currentFrame + 1) % Renderer::s_maxFramesInFlight;
}

void Renderer::bindDescriptorSets()
{
    vkCmdBindDescriptorSets(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline.layout /*m_graphicsPipeline->getLayout()*/,
        0, static_cast<uint32_t>(sizeof(m_currentDescriptorSets) / sizeof(m_currentDescriptorSets[0])), m_currentDescriptorSets, 0, nullptr);
}

void Renderer::createCommandBuffers()
{
    m_commandBuffers.resize(Renderer::s_maxFramesInFlight);

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

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline.pipeline /*m_graphicsPipeline->get()*/);

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

void Renderer::createGraphicsPipeline()
{
    VkDescriptorSetLayout layouts[] = {
        m_globalDescriptorSetLayout->get(),
        m_modelDescriptorSetLayout->get(),
        m_textureDescriptorSetLayout->get()
    };

    VkPipelineLayoutCreateInfo mesh_layout_info = RendererInits::pipelineLayoutCreateInfo();
    mesh_layout_info.setLayoutCount = sizeof(layouts) / sizeof(layouts[0]);
    mesh_layout_info.pSetLayouts = layouts;
    mesh_layout_info.pPushConstantRanges = nullptr;
    mesh_layout_info.pushConstantRangeCount = 0;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(m_device->get(), &mesh_layout_info, nullptr, &newLayout));
    
    m_graphicsPipeline.layout = newLayout;

    PipelineBuilder builder(this);

    // Ustaw modu³y shaderów
    VkShaderModule vertShaderModule = compileShader("shaders/shader.vert", shaderc_vertex_shader, m_device->get());
    VkShaderModule fragShaderModule = compileShader("shaders/shader.frag", shaderc_fragment_shader, m_device->get());
    
    builder.setShaders(vertShaderModule, fragShaderModule);

    // Ustaw topologiê wejœciow¹
    builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    // Ustaw tryb poligonu
    builder.setPolygonMode(VK_POLYGON_MODE_FILL);

    // Ustaw tryb usuwania tylnych œcian
    builder.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

    // Wy³¹cz próbkowanie wielokrotne
    builder.setMultisampling(s_msaaSamples, 0.2f);

    // Wy³¹cz mieszanie kolorów
    builder.disableBlending();

    // Ustaw formaty za³¹czników
    builder.setColorAttachmentFormat(m_swapChain->m_swapChainImageFormat);
    builder.setDepthFormat(m_device->findDepthFormat());

    // W³¹cz test g³êbi
    builder.enableDepthtest(true, VK_COMPARE_OP_LESS);

    // Zbuduj potok
    m_graphicsPipeline.pipeline = builder.buildPipeline(m_graphicsPipeline.layout, m_device->get());

    vkDestroyShaderModule(m_device->get(), vertShaderModule, nullptr);
    vkDestroyShaderModule(m_device->get(), fragShaderModule, nullptr);
}
