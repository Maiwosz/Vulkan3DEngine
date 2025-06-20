#include "ImGuiWrapper.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

ImGuiWrapper::ImGuiWrapper(
    Window& window,
    VulkanContext& context,
    RenderPassHandle renderPassHandle,
    RenderPassManager& renderPassManager,
    uint32_t minImageCount,
    uint32_t imageCount,
    VkSampleCountFlagBits msaaSamples
)
    : m_window(window),
    m_context(context),
    m_renderPassHandle(renderPassHandle),
    m_renderPassManager(renderPassManager),
    m_minImageCount(minImageCount),
    m_imageCount(imageCount),
    m_msaaSamples(msaaSamples)
{
    init();
}

ImGuiWrapper::~ImGuiWrapper() {
    shutdown();
}

VkRenderPass ImGuiWrapper::getCurrentRenderPass() const {
    VkRenderPass* renderPassPtr = m_renderPassManager.getResource(m_renderPassHandle);
    if (!renderPassPtr) {
        throw std::runtime_error("Invalid render pass handle in ImGuiWrapper");
    }
    return *renderPassPtr;
}

void ImGuiWrapper::init() {
    if (m_initialized) return;

    createDescriptorPool();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Konfiguracja IO
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();

    // Inicjalizacja backendów
    ImGui_ImplGlfw_InitForVulkan(m_window.get(), true);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = m_context.instance().get();
    initInfo.PhysicalDevice = m_context.physical().get();
    initInfo.Device = m_context.logical().get();

    // Pobieranie indeksu rodziny kolejek z LogicalDevice
    initInfo.QueueFamily = m_context.logical().getQueueFamilyIndex(LogicalDevice::QueueType::Graphics);
    initInfo.Queue = m_context.logical().getQueue(LogicalDevice::QueueType::Graphics);

    initInfo.DescriptorPool = m_imguiPool;
    initInfo.MinImageCount = m_minImageCount;
    initInfo.ImageCount = m_imageCount;
    initInfo.MSAASamples = m_msaaSamples;
    initInfo.RenderPass = getCurrentRenderPass();  // Używamy aktualnego render passa

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();

    m_initialized = true;
}

void ImGuiWrapper::recreate() {
    vkDeviceWaitIdle(m_context.logical().get());
    shutdown();
    init();  // Automatycznie użyje aktualnego render passa
}

void ImGuiWrapper::render(VkCommandBuffer commandBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer, 0);
}

void ImGuiWrapper::createDescriptorPool() {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(
        m_context.logical().get(),
        &pool_info,
        nullptr,
        &m_imguiPool
    ));
}

void ImGuiWrapper::shutdown() {
    if (!m_initialized) return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_imguiPool) {
        vkDestroyDescriptorPool(
            m_context.logical().get(),
            m_imguiPool,
            nullptr
        );
        m_imguiPool = VK_NULL_HANDLE;
    }

    m_initialized = false;
}