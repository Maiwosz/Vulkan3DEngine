#include "DearImGuiProvider.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

DearImGuiProvider::DearImGuiProvider(Window& window, VulkanContext& context, SwapChain& swapchain)
    : m_window(window)
    , m_context(context)
    , m_swapchain(swapchain) {
    
    initializeGlobal();
}

DearImGuiProvider::~DearImGuiProvider() {
    shutdown();
    shutdownGlobal();
}

void DearImGuiProvider::initializeGlobal() {
    if (m_globalInitialized) return;

    SPDLOG_DEBUG("DearImGuiProvider: Initializing global ImGui context");

    // Create descriptor pool
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

    // Initialize GLFW backend
    ImGui_ImplGlfw_InitForVulkan(m_window.get(), true);

    m_globalInitialized = true;
    SPDLOG_DEBUG("DearImGuiProvider: Global initialization complete");
}

void DearImGuiProvider::shutdownGlobal() {
    if (!m_globalInitialized) return;

    SPDLOG_DEBUG("DearImGuiProvider: Shutting down global ImGui context");

    vkDeviceWaitIdle(m_context.logical().get());

    // Shutdown GLFW backend
    ImGui_ImplGlfw_Shutdown();

    // Destroy ImGui context
    ImGui::DestroyContext();

    // Destroy descriptor pool
    destroyDescriptorPool();

    m_globalInitialized = false;
    SPDLOG_DEBUG("DearImGuiProvider: Global shutdown complete");
}

void DearImGuiProvider::createDescriptorPool() {
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
        &m_descriptorPool
    ));
}

void DearImGuiProvider::destroyDescriptorPool() {
    if (m_descriptorPool) {
        vkDestroyDescriptorPool(
            m_context.logical().get(),
            m_descriptorPool,
            nullptr
        );
        m_descriptorPool = VK_NULL_HANDLE;
    }
}

bool DearImGuiProvider::initialize(SmartRenderPassHandle renderPass, VkSampleCountFlagBits msaaSamples) {
    if (!m_globalInitialized) {
        SPDLOG_ERROR("DearImGuiProvider: Cannot initialize - global context not ready");
        return false;
    }

    // Check if we need to reinitialize
    bool needsReinit = false;
    
    if (m_initialized) {
        // Compare render passes
        VkRenderPass* currentRP = m_currentRenderPass.get();
        VkRenderPass* newRP = renderPass.get();
        
        if (!currentRP || !newRP || *currentRP != *newRP || m_currentMSAASamples != msaaSamples) {
            SPDLOG_DEBUG("DearImGuiProvider: Render pass changed, reinitializing");
            needsReinit = true;
            shutdownVulkanBackend();
        }
    }

    if (!m_initialized || needsReinit) {
        m_currentRenderPass = renderPass;
        m_currentMSAASamples = msaaSamples;
        
        if (!initializeVulkanBackend()) {
            SPDLOG_ERROR("DearImGuiProvider: Failed to initialize Vulkan backend");
            return false;
        }
    }

    return true;
}

bool DearImGuiProvider::initializeVulkanBackend() {
    VkRenderPass* renderPassPtr = m_currentRenderPass.get();
    if (!renderPassPtr) {
        SPDLOG_ERROR("DearImGuiProvider: Invalid render pass handle");
        return false;
    }

    SPDLOG_DEBUG("DearImGuiProvider: Initializing Vulkan backend");

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = m_context.instance().get();
    initInfo.PhysicalDevice = m_context.physical().get();
    initInfo.Device = m_context.logical().get();
    initInfo.QueueFamily = m_context.logical().getQueueFamilyIndex(LogicalDevice::QueueType::Graphics);
    initInfo.Queue = m_context.logical().getQueue(LogicalDevice::QueueType::Graphics);
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.MinImageCount = m_context.physical().getMinImageCount();
    initInfo.ImageCount = m_swapchain.getFramesInFlight();
    initInfo.MSAASamples = m_currentMSAASamples;
    initInfo.RenderPass = *renderPassPtr;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        SPDLOG_ERROR("DearImGuiProvider: ImGui_ImplVulkan_Init failed");
        return false;
    }

    // Upload fonts
    ImGui_ImplVulkan_CreateFontsTexture();

    m_initialized = true;
    SPDLOG_DEBUG("DearImGuiProvider: Vulkan backend initialized successfully");
    return true;
}

void DearImGuiProvider::shutdownVulkanBackend() {
    if (!m_initialized) return;

    SPDLOG_DEBUG("DearImGuiProvider: Shutting down Vulkan backend");

    vkDeviceWaitIdle(m_context.logical().get());
    ImGui_ImplVulkan_Shutdown();

    m_initialized = false;
    m_currentRenderPass = SmartRenderPassHandle();
}

void DearImGuiProvider::shutdown() {
    shutdownVulkanBackend();
}

void DearImGuiProvider::beginFrame() {
    if (!m_globalInitialized) {
        SPDLOG_WARN("DearImGuiProvider: beginFrame called but not initialized");
        return;
    }

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void DearImGuiProvider::endFrame() {
    if (!m_globalInitialized) {
        return;
    }

    ImGui::Render();
}

void DearImGuiProvider::render(VkCommandBuffer commandBuffer) {
    if (!m_initialized) {
        SPDLOG_WARN("DearImGuiProvider: render called but Vulkan backend not initialized");
        return;
    }

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer, VK_NULL_HANDLE);
    }
}

uint32_t DearImGuiProvider::registerCallback(ImGuiCallback callback) {
    if (!callback) {
        SPDLOG_WARN("DearImGuiProvider: Attempted to register null callback");
        return 0;
    }

    uint32_t id = ++m_nextCallbackId;
    m_callbacks[id] = std::move(callback);
    
    SPDLOG_DEBUG("DearImGuiProvider: Registered callback with ID {}", id);
    return id;
}

bool DearImGuiProvider::unregisterCallback(uint32_t callbackId) {
    auto it = m_callbacks.find(callbackId);
    if (it != m_callbacks.end()) {
        m_callbacks.erase(it);
        SPDLOG_DEBUG("DearImGuiProvider: Unregistered callback with ID {}", callbackId);
        return true;
    }
    
    SPDLOG_WARN("DearImGuiProvider: Callback with ID {} not found", callbackId);
    return false;
}

void DearImGuiProvider::clearCallbacks() {
    size_t count = m_callbacks.size();
    m_callbacks.clear();
    SPDLOG_DEBUG("DearImGuiProvider: Cleared {} callbacks", count);
}

void DearImGuiProvider::executeCallbacks() {
    for (const auto& [id, callback] : m_callbacks) {
        try {
            callback();
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("DearImGuiProvider: Exception in callback {}: {}", id, e.what());
        }
    }
}