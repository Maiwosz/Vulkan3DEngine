#include "ImGuiRenderPass.h"
#include "ImGuiCore.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"

ImGuiRenderPass::ImGuiRenderPass(
    ImGuiCore& imguiContext,
    const VulkanContext& context,
    VkDescriptorPool pool,
    SmartRenderPassHandle renderPass,
    uint32_t imageCount,
    VkSampleCountFlagBits msaaSamples
)
    : m_imguiContext(imguiContext),
    m_context(context),
    m_descriptorPool(pool),
    m_renderPass(renderPass),
    m_imageCount(imageCount),
    m_msaaSamples(msaaSamples)
{
    init();
}

ImGuiRenderPass::~ImGuiRenderPass() {
    shutdown();
    // SmartHandle automatycznie zwolni referencję w swoim destruktorze
}

VkRenderPass ImGuiRenderPass::getCurrentRenderPass() const {
    VkRenderPass* renderPassPtr = m_renderPass.get();
    if (!renderPassPtr) {
        throw std::runtime_error("Invalid render pass handle in ImGuiRenderPass");
    }
    return *renderPassPtr;
}

void ImGuiRenderPass::init() {
    if (m_initialized) return;

    // Initialize Vulkan backend for this specific render pass
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = m_context.instance().get();
    initInfo.PhysicalDevice = m_context.physical().get();
    initInfo.Device = m_context.logical().get();
    initInfo.QueueFamily = m_context.logical().getQueueFamilyIndex(LogicalDevice::QueueType::Graphics);
    initInfo.Queue = m_context.logical().getQueue(LogicalDevice::QueueType::Graphics);
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.MinImageCount = m_context.physical().getMinImageCount();
    initInfo.ImageCount = m_imageCount;
    initInfo.MSAASamples = m_msaaSamples;
    initInfo.RenderPass = getCurrentRenderPass();

    // Initialize Vulkan backend
    ImGui_ImplVulkan_Init(&initInfo);

    // Upload fonts (only needs to be done once globally, but ImGui_ImplVulkan_Init expects it)
    // This is safe to call multiple times - ImGui handles it internally
    ImGui_ImplVulkan_CreateFontsTexture();

    m_initialized = true;
}

void ImGuiRenderPass::shutdown() {
    if (!m_initialized) return;

    vkDeviceWaitIdle(m_context.logical().get());

    // Shutdown only the Vulkan backend for this render pass
    ImGui_ImplVulkan_Shutdown();

    m_initialized = false;
}

void ImGuiRenderPass::recreate() {
    shutdown();
    init();
}

void ImGuiRenderPass::render(VkCommandBuffer commandBuffer) {
    if (!m_initialized) {
        throw std::runtime_error("ImGuiRenderPass::render called before initialization");
    }

    // Note: ImGui::Render() should be called once per frame in the main loop,
    // not here. This function only submits the draw data to the command buffer.
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer, VK_NULL_HANDLE);
    }
}