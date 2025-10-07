#include "ImGuiCore.h"
#include "ImGuiRenderPass.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

ImGuiCore::ImGuiCore(Window& window, VulkanContext& context, SwapChain& swapchain)
    : m_window(window),
    m_context(context),
    m_swapchain(swapchain)
{
    initGlobalImGui();
}

ImGuiCore::~ImGuiCore() {
    shutdownGlobalImGui();
}

void ImGuiCore::initGlobalImGui() {
    if (m_initialized) return;

    // Create descriptor pool for ImGui
    createDescriptorPool();

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Configure IO
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Set style
    ImGui::StyleColorsDark();

    // Initialize GLFW backend (global, happens once)
    ImGui_ImplGlfw_InitForVulkan(m_window.get(), true);

    m_initialized = true;
}

void ImGuiCore::shutdownGlobalImGui() {
    if (!m_initialized) return;

    // Wait for device to finish
    vkDeviceWaitIdle(m_context.logical().get());

    // Shutdown GLFW backend
    ImGui_ImplGlfw_Shutdown();

    // Destroy ImGui context
    ImGui::DestroyContext();

    // Destroy descriptor pool
    if (m_descriptorPool) {
        vkDestroyDescriptorPool(
            m_context.logical().get(),
            m_descriptorPool,
            nullptr
        );
        m_descriptorPool = VK_NULL_HANDLE;
    }

    m_initialized = false;
}

void ImGuiCore::createDescriptorPool() {
    // Create a descriptor pool with enough space for multiple render passes
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
        { VK_DESCRIPTOR_TYPE_SAMPLER, 10 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 10;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(
        m_context.logical().get(),
        &pool_info,
        nullptr,
        &m_descriptorPool
    ));
}

std::unique_ptr<ImGuiRenderPass> ImGuiCore::createRenderPass(
    SmartRenderPassHandle renderPass,
    VkSampleCountFlagBits msaaSamples
) {
    return std::make_unique<ImGuiRenderPass>(
        *this,
        m_context,
        m_descriptorPool,
        renderPass,
        m_swapchain.getFramesInFlight(),
        msaaSamples
    );
}

void ImGuiCore::newFrame() {
    // Start new ImGui frame (call once per frame)
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}