#include "Renderer.h"
#include "StagingBuffer.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "UniformBuffer.h"
#include "Image.h"

#include <ranges>

#include "Application.h"
#include "RendererInits.h"

VkSampleCountFlagBits Renderer::s_maxMsaaSamples = VK_SAMPLE_COUNT_1_BIT;
VkSampleCountFlagBits Renderer::s_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
int Renderer::s_framesInFlight = 2;

Renderer::Renderer()
{
    try {
        m_swapChain = std::make_shared<SwapChain>(this);
    }
    catch (...) { throw std::exception("SwapChain not created successfully"); }

    createGraphicsPipeline();
    createPointLightPipeline();
    
    createCommandBuffers();

    initImgui();
}

Renderer::~Renderer()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(GraphicsEngine::get()->getDevice()->get(), m_imguiPool, nullptr);
    
    m_descriptorAllocator.reset();
    m_graphicsPipeline.reset();
    m_pointLightPipeline.reset();

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

void Renderer::drawFrame()
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

    recordCommandBuffer(m_commandBuffers[m_currentFrame], m_currentImageIndex);

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

    result = vkQueuePresentKHR(GraphicsEngine::get()->getDevice()->getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || Window::get()->wasWindowResized()) {
        Window::get()->resetWindowResizedFlag();
        m_swapChain->recreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % Renderer::s_framesInFlight;
}

void Renderer::drawModel(Model* model)
{
    m_modelDraws.push_back(model);
}

void Renderer::bindDescriptorSet(VkDescriptorSet set, int position)
{
    m_currentDescriptorSets[position] = set;
}

void Renderer::recreatePipelines()
{
    vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());
    m_graphicsPipeline.reset();
    m_pointLightPipeline.reset();

    vkDestroyDescriptorSetLayout(GraphicsEngine::get()->getDevice()->get(), m_globalDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(GraphicsEngine::get()->getDevice()->get(), m_modelDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(GraphicsEngine::get()->getDevice()->get(), m_textureDescriptorSetLayout, nullptr);

    createGraphicsPipeline();
    createPointLightPipeline();
}

void Renderer::recreateImgui()
{
    // Wait for device to be idle to ensure no resources are in use
    vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());

    // Shutdown ImGui and free its resources
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Destroy ImGui's descriptor pool
    vkDestroyDescriptorPool(GraphicsEngine::get()->getDevice()->get(), m_imguiPool, nullptr);

    // Reinitialize ImGui
    initImgui();
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

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Imgui
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
    if (m_modelDraws.size() > 0) {
        m_currentDescriptorSets[0] = GraphicsEngine::get()->getScene()->m_globalDescriptorSets[m_currentFrame];

        std::ranges::for_each(m_modelDraws, [&](auto m) {
            m_currentDescriptorSets[1] = m->m_descriptorSets[GraphicsEngine::get()->getRenderer()->getCurrentFrame()];

            m->m_mesh->m_vertexBuffer->bind();
            if (m->m_mesh->m_hasIndexBuffer) {
                m->m_mesh->m_indexBuffer->bind();
            }

            if (m->m_texture) {
                m_currentDescriptorSets[2] = m->m_texture->m_descriptorSets[GraphicsEngine::get()->getRenderer()->getCurrentFrame()];
            }

            vkCmdBindDescriptorSets(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline->layout,
                0, static_cast<uint32_t>(sizeof(m_currentDescriptorSets) / sizeof(m_currentDescriptorSets[0])), m_currentDescriptorSets, 0, nullptr);

            if (m->m_mesh->m_hasIndexBuffer) {
                vkCmdDrawIndexed(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(),
                    static_cast<uint32_t>(m->m_mesh->getIndicesSize()), 1, 0, 0, 0);
            }
            else {
                vkCmdDraw(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), 3, 1, 0, 0);
            }
            });

        m_modelDraws.clear();
    }

    if (GraphicsEngine::get()->getScene()->m_pointLights.size()>0) {
        m_currentGlobalDescriptorSet[0] = m_currentDescriptorSets[0];

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pointLightPipeline->pipeline);
        vkCmdBindDescriptorSets(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pointLightPipeline->layout,
            0, 1, m_currentGlobalDescriptorSet, 0, nullptr);
        vkCmdDraw(commandBuffer, 6, GraphicsEngine::get()->getScene()->m_pointLights.size(), 0, 0);
    }

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffers[m_currentFrame], 0);

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

    // Ustaw modu�y shader�w
    VkShaderModule vertShaderModule = compileShader("shaders/shader.vert", shaderc_vertex_shader, GraphicsEngine::get()->getDevice()->get());
    VkShaderModule fragShaderModule = compileShader("shaders/shader.frag", shaderc_fragment_shader, GraphicsEngine::get()->getDevice()->get());
    
    builder.setShaders(vertShaderModule, fragShaderModule);

    // Ustaw topologi� wej�ciow�
    builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    // Ustaw tryb poligonu
    builder.setPolygonMode(VK_POLYGON_MODE_FILL);

    // Ustaw tryb usuwania tylnych �cian
    builder.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

    // Wy��cz pr�bkowanie wielokrotne
    builder.setMultisampling(s_msaaSamples, 0.2f);

    // Wy��cz mieszanie kolor�w
    builder.disableBlending();

    // Ustaw formaty za��cznik�w
    builder.setColorAttachmentFormat(m_swapChain->m_swapChainImageFormat);
    builder.setDepthFormat(GraphicsEngine::get()->getDevice()->findDepthFormat());

    // W��cz test g��bi
    builder.enableDepthtest(true, VK_COMPARE_OP_LESS);

    // Zbuduj potok
    m_graphicsPipeline->pipeline = builder.buildPipeline(m_graphicsPipeline->layout, GraphicsEngine::get()->getDevice()->get());

    vkDestroyShaderModule(GraphicsEngine::get()->getDevice()->get(), vertShaderModule, nullptr);
    vkDestroyShaderModule(GraphicsEngine::get()->getDevice()->get(), fragShaderModule, nullptr);
}

void Renderer::createPointLightPipeline()
{
    m_pointLightPipeline = std::make_unique<Pipeline>(&GraphicsEngine::get()->getDevice()->get());

    std::vector <VkDescriptorSetLayout> layouts;

    layouts.push_back(m_globalDescriptorSetLayout);

    VkPipelineLayoutCreateInfo billboard_layout_info = RendererInits::pipelineLayoutCreateInfo();
    billboard_layout_info.setLayoutCount = layouts.size();
    billboard_layout_info.pSetLayouts = layouts.data();
    billboard_layout_info.pPushConstantRanges = nullptr;
    billboard_layout_info.pushConstantRangeCount = 0;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(GraphicsEngine::get()->getDevice()->get(), &billboard_layout_info, nullptr, &newLayout));

    m_pointLightPipeline->layout = newLayout;

    PipelineBuilder builder(this);

    // Ustaw modu�y shader�w dla billboard�w
    VkShaderModule vertShaderModule = compileShader("shaders/pointLight.vert", shaderc_vertex_shader, GraphicsEngine::get()->getDevice()->get());
    VkShaderModule fragShaderModule = compileShader("shaders/pointLight.frag", shaderc_fragment_shader, GraphicsEngine::get()->getDevice()->get());

    builder.setShaders(vertShaderModule, fragShaderModule);

    // Ustaw topologi� wej�ciow� na punkty
    builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    // Ustaw tryb poligonu na wype�nienie
    builder.setPolygonMode(VK_POLYGON_MODE_FILL);

    // Wy��cz usuwanie tylnych �cian
    builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

    // Wy��cz pr�bkowanie wielokrotne
    builder.setMultisampling(s_msaaSamples, 0.2f);

    // Wy��cz mieszanie kolor�w
    //builder.disableBlending();
    builder.enableBlendingAlphablend();

    // Ustaw formaty za��cznik�w
    builder.setColorAttachmentFormat(m_swapChain->m_swapChainImageFormat);
    builder.setDepthFormat(GraphicsEngine::get()->getDevice()->findDepthFormat());

    // Wy��cz test g��bi
    builder.enableDepthtest(false, VK_COMPARE_OP_LESS);

    // Zbuduj potok
    m_pointLightPipeline->pipeline = builder.buildPipeline(m_pointLightPipeline->layout, GraphicsEngine::get()->getDevice()->get());

    vkDestroyShaderModule(GraphicsEngine::get()->getDevice()->get(), vertShaderModule, nullptr);
    vkDestroyShaderModule(GraphicsEngine::get()->getDevice()->get(), fragShaderModule, nullptr);
}

void Renderer::initImgui()
{

    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(GraphicsEngine::get()->getDevice()->get(), &pool_info, nullptr, &m_imguiPool));

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(Window::get()->getWindow(), true);
    ImGui_ImplVulkan_InitInfo info;
    info.Instance = Instance::get()->getVkInstance();
    info.PhysicalDevice = GraphicsEngine::get()->getDevice()->getPhysicalDevice();
    info.Device = GraphicsEngine::get()->getDevice()->get();
    info.QueueFamily = GraphicsEngine::get()->getDevice()->findQueueFamilies(GraphicsEngine::get()->getDevice()->getPhysicalDevice()).graphicsFamily.value();
    info.Queue = GraphicsEngine::get()->getDevice()->getGraphicsQueue();
    info.PipelineCache = VK_NULL_HANDLE;
    info.DescriptorPool = m_imguiPool;
    info.MinAllocationSize = 1024 * 1024;
    info.Allocator = nullptr;
    info.MinImageCount = s_maxFramesInFlight;//???
    info.ImageCount = s_maxFramesInFlight;
    info.MSAASamples = s_msaaSamples;
    info.RenderPass = m_swapChain->getRenderPass();
    info.Subpass = 0;
    info.CheckVkResultFn = nullptr;
    info.UseDynamicRendering = false;
    ImGui_ImplVulkan_Init(&info);

    ImGui_ImplVulkan_CreateFontsTexture();

    vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());
}
