#include "Renderer.h"
#include "StagingBuffer.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "UniformBuffer.h"
#include "Image.h"

#include "Application.h"
#include "RendererInits.h"

VkSampleCountFlagBits Renderer::s_msaaSamples = VK_SAMPLE_COUNT_1_BIT;

Renderer::Renderer()
{
    try {
        m_swapChain = std::make_shared<SwapChain>(this);
    }
    catch (...) { throw std::exception("SwapChain not created successfully"); }

    createGraphicsPipeline();
    
    createCommandBuffers();
}

Renderer::~Renderer()
{
    m_descriptorAllocator.reset();
    m_graphicsPipeline.reset();
    vkDestroyDescriptorSetLayout(GraphicsEngine::get()->getDevice()->get(), m_globalDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(GraphicsEngine::get()->getDevice()->get(), m_modelDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(GraphicsEngine::get()->getDevice()->get(), m_textureDescriptorSetLayout, nullptr);
    m_swapChain.reset();
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

void Renderer::drawFrameBegin()
{
    vkWaitForFences(GraphicsEngine::get()->getDevice()->get(), 1, &m_swapChain->m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    
    VkResult result = vkAcquireNextImageKHR(GraphicsEngine::get()->getDevice()->get(), m_swapChain->m_swapChain, UINT64_MAX,
        m_swapChain->m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
         m_swapChain->recreateSwapChain();
         return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
         throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(GraphicsEngine::get()->getDevice()->get(), 1, &m_swapChain->m_inFlightFences[m_currentFrame]);

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

    if (vkQueueSubmit(GraphicsEngine::get()->getDevice()->getGraphicsQueue(), 1, &submitInfo, m_swapChain->m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
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

    VkResult result = vkQueuePresentKHR(GraphicsEngine::get()->getDevice()->getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || GraphicsEngine::get()->getWindow()->wasWindowResized()) {
        GraphicsEngine::get()->getWindow()->resetWindowResizedFlag();
        m_swapChain->recreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % Renderer::s_maxFramesInFlight;
}

void Renderer::bindDescriptorSet(VkDescriptorSet set, int position)
{
    m_currentDescriptorSets[position] = set;
}

void Renderer::bindDescriptorSets()
{
    vkCmdBindDescriptorSets(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline->layout,
        0, static_cast<uint32_t>(sizeof(m_currentDescriptorSets) / sizeof(m_currentDescriptorSets[0])), m_currentDescriptorSets, 0, nullptr);
}

void Renderer::createCommandBuffers()
{
    m_commandBuffers.resize(Renderer::s_maxFramesInFlight);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = GraphicsEngine::get()->getDevice()->getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

    if (vkAllocateCommandBuffers(GraphicsEngine::get()->getDevice()->get(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
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

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline->pipeline);

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
    m_graphicsPipeline = std::make_unique<Pipeline>(&GraphicsEngine::get()->getDevice()->get());

    std::vector <VkDescriptorSetLayout> layouts;

    DescriptorLayoutBuilder layoutBuilder;

    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_globalDescriptorSetLayout = layoutBuilder.build(GraphicsEngine::get()->getDevice()->get(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    layouts.push_back(m_globalDescriptorSetLayout);

    m_modelDescriptorSetLayout = layoutBuilder.build(GraphicsEngine::get()->getDevice()->get(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    layouts.push_back(m_modelDescriptorSetLayout);

    layoutBuilder.clear();

    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_textureDescriptorSetLayout = layoutBuilder.build(GraphicsEngine::get()->getDevice()->get(), VK_SHADER_STAGE_FRAGMENT_BIT);

    layouts.push_back(m_textureDescriptorSetLayout);

    VkPipelineLayoutCreateInfo mesh_layout_info = RendererInits::pipelineLayoutCreateInfo();
    mesh_layout_info.setLayoutCount = layouts.size();
    mesh_layout_info.pSetLayouts = layouts.data();
    mesh_layout_info.pPushConstantRanges = nullptr;
    mesh_layout_info.pushConstantRangeCount = 0;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(GraphicsEngine::get()->getDevice()->get(), &mesh_layout_info, nullptr, &newLayout));
    
    m_graphicsPipeline->layout = newLayout;

    PipelineBuilder builder(this);

    // Ustaw modu³y shaderów
    VkShaderModule vertShaderModule = compileShader("shaders/shader.vert", shaderc_vertex_shader, GraphicsEngine::get()->getDevice()->get());
    VkShaderModule fragShaderModule = compileShader("shaders/shader.frag", shaderc_fragment_shader, GraphicsEngine::get()->getDevice()->get());
    
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
    builder.setDepthFormat(GraphicsEngine::get()->getDevice()->findDepthFormat());

    // W³¹cz test g³êbi
    builder.enableDepthtest(true, VK_COMPARE_OP_LESS);

    // Zbuduj potok
    m_graphicsPipeline->pipeline = builder.buildPipeline(m_graphicsPipeline->layout, GraphicsEngine::get()->getDevice()->get());

    vkDestroyShaderModule(GraphicsEngine::get()->getDevice()->get(), vertShaderModule, nullptr);
    vkDestroyShaderModule(GraphicsEngine::get()->getDevice()->get(), fragShaderModule, nullptr);
}
