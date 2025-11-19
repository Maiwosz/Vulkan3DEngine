#include "EngineCore.h"
#include "VulkanUtilities.h"
#include <stdexcept>
#include "Engine.h"
#include "GraphicsTypes.h"

// Zdefiniuj wymagania Vulkan dla silnika
VulkanRequirements createEngineRequirements() {
    VulkanRequirements req;

    // ==================== Podstawowa konfiguracja ====================
    req.minimumApiVersion = {
        VK_API_VERSION_1_1,
        "Vulkan 1.1 required for VkPhysicalDeviceFeatures2"
    };

#ifdef _DEBUG
    req.enableValidation = true;
    req.enableDebugPrintf = false;
#else
    req.enableValidation = false;
    req.enableDebugPrintf = false;
#endif

    // ==================== Warstwy ====================
    if (req.enableValidation) {
        req.validationLayers.emplace_back(
            "VK_LAYER_KHRONOS_validation",
            RequirementLevel::Required
        );
    }

    // ==================== Rozszerzenia instancji ====================
    // GLFW extensions
    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    for (uint32_t i = 0; i < glfwExtCount; ++i) {
        req.requireInstanceExtension(glfwExts[i]);
    }

    if (req.enableValidation) {
        req.requireInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // ==================== Rozszerzenia urządzenia ====================
    req.requireDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    if (req.enableDebugPrintf) {
        req.requireDeviceExtension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
    }

    // Preferowane nowoczesne rozszerzenia
    req.preferDeviceExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, 100);
    req.preferDeviceExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, 150);

    // Atomic float extension
    // Niektóre karty (np. AMD) wspierają rozszerzenie tylko częściowo
    req.preferDeviceExtension(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);

    // ==================== Feature'y 1.0 ====================
    req.requireFeature("samplerAnisotropy",
        &VkPhysicalDeviceFeatures::samplerAnisotropy);

    req.preferFeature("sampleRateShading",
        &VkPhysicalDeviceFeatures::sampleRateShading, 50);

    req.preferFeature("fillModeNonSolid",
        &VkPhysicalDeviceFeatures::fillModeNonSolid, 25);

    req.deviceFeatures.emplace_back(
        "geometryShader",
        RequirementLevel::Optional,
        &VkPhysicalDeviceFeatures::geometryShader,
        100
    );

    req.deviceFeatures.emplace_back(
        "tessellationShader",
        RequirementLevel::Optional,
        &VkPhysicalDeviceFeatures::tessellationShader,
        100
    );

    // ==================== Extension Features ====================

    // Buffer atomics
    req.preferExtensionFeature<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>(
        "shaderBufferFloat32Atomics",
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
        &VkPhysicalDeviceShaderAtomicFloatFeaturesEXT::shaderBufferFloat32Atomics,
        75  // Zwiększ score jeśli dostępne
    );

    req.preferExtensionFeature<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>(
        "shaderBufferFloat32AtomicAdd",
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
        &VkPhysicalDeviceShaderAtomicFloatFeaturesEXT::shaderBufferFloat32AtomicAdd,
        75  // Zwiększ score jeśli dostępne
    );

    // Shared memory atomics (optional, but useful for compute shaders)
    req.preferExtensionFeature<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>(
        "shaderSharedFloat32Atomics",
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
        &VkPhysicalDeviceShaderAtomicFloatFeaturesEXT::shaderSharedFloat32Atomics,
        50
    );

    req.preferExtensionFeature<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>(
        "shaderSharedFloat32AtomicAdd",
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
        &VkPhysicalDeviceShaderAtomicFloatFeaturesEXT::shaderSharedFloat32AtomicAdd,
        50
    );

    // ==================== Kolejki ====================
    req.requireQueue("Graphics", VK_QUEUE_GRAPHICS_BIT, false);
    req.requireQueue("Transfer", VK_QUEUE_TRANSFER_BIT, true, 50);
    req.requireQueue("Compute", VK_QUEUE_COMPUTE_BIT, true, 75);

    return req;
}

EngineCore::EngineCore(Settings& settings, Window& window)
    : m_settings(settings), m_window(window) {
    try {
        // Stwórz wymagania
        VulkanRequirements requirements = createEngineRequirements();

        // Stwórz kontekst Vulkan (automatycznie waliduje wymagania)
        m_vulkanContext = std::make_unique<VulkanContext>(
            requirements,
            m_window,
            m_settings
        );

        // Initialize low-level managers
        m_commandBufferManager = std::make_unique<CommandBufferManager>(*m_vulkanContext);
        m_syncResourceManager = std::make_unique<SynchronizationResourceManager>(m_vulkanContext->logical());
        m_frameManager = std::make_unique<FrameManager>(
            *m_vulkanContext,
            *m_syncResourceManager,
            *m_commandBufferManager,
            m_settings.getFramesInFlight()
        );
        m_vramManager = std::make_unique<VramManager>(
            *m_vulkanContext,
            *m_frameManager,
            *m_commandBufferManager,
            *m_syncResourceManager
        );
        m_shaderModuleManager = std::make_unique<ShaderModuleManager>(
            m_vulkanContext->logical()
        );
        m_descriptorLayoutManager = std::make_unique<DescriptorLayoutManager>(
            m_vulkanContext->logical()
        );
        m_bufferManager = std::make_unique<BufferManager>(
            *m_vramManager
        );
        m_pipelineLayoutManager = std::make_unique<PipelineLayoutManager>(
            m_vulkanContext->logical()
        );
        m_attachmentManager = std::make_unique<AttachmentManager>(
            m_vulkanContext->logical(),
            *m_vramManager
        );
        m_swapChain = std::make_unique<SwapChain>(
            m_vulkanContext->surface(),
            m_vulkanContext->physical(),
            m_vulkanContext->logical(),
            *m_vramManager,
            *m_attachmentManager,
            window,
            settings
        );

        SamplerSettings samplerSettings;
        samplerSettings.textureFiltering = m_settings.getTextureFiltering();
        samplerSettings.mipmapMode = m_settings.getMipmapMode();
        samplerSettings.maxAnisotropy = m_settings.getCurrentAnisotropyValue();
        samplerSettings.anisotropySupported = m_settings.getHardwareLimits().anisotropySupported;

        m_samplerManager = std::make_unique<ImageSamplerManager>(
            m_vulkanContext->logical(),
            samplerSettings
        );

        // Initialize resource managers
        m_renderPassManager = std::make_unique<RenderPassManager>(
            m_vulkanContext->logical()
        );
        m_framebufferManager = std::make_unique<FrameBufferManager>(
            m_vulkanContext->logical(),
            *m_renderPassManager,
            *m_attachmentManager
        );
        m_pipelineManager = std::make_unique<PipelineManager>(
            m_vulkanContext->logical(),
            *m_shaderModuleManager,
            *m_pipelineLayoutManager
        );

        DescriptorAllocator::PoolConfig allocConfig;
        allocConfig.initialSets = 512;
        allocConfig.ratios = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.0f }
        };
        allocConfig.growthFactor = 1.5f;

        m_descriptorAllocator = std::make_unique<DescriptorAllocator>(
            m_vulkanContext->logical(),
            allocConfig
        );

        m_renderGraphManager = std::make_unique<RenderGraphManager>(
            *m_attachmentManager,
			*m_renderPassManager
        );

        // Create renderer
        m_renderer = std::make_unique<Renderer>(
            *this,
            *m_vulkanContext,
            *m_frameManager,
            *m_vramManager,
            *m_swapChain,
            *m_attachmentManager,
            *m_framebufferManager,
            *m_renderPassManager,
            *m_descriptorAllocator,
            *m_pipelineManager,
			*m_commandBufferManager,
            *m_syncResourceManager
        );

    }
    catch (const std::exception& e) {
        throw std::runtime_error("Renderer initialization failed: " + std::string(e.what()));
    }
}

EngineCore::~EngineCore() {
    // Wait for all operations to complete before destroying resources
    if (m_vulkanContext && m_vulkanContext->logical().get() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_vulkanContext->logical().get());
    }

    // Reset frame manager to ensure no pending frames hold references
    if (m_frameManager) {
        m_frameManager->waitForAllFrames();
    }

    // Destroy managers in proper order
    m_renderer.reset();
    m_frameManager.reset();
    m_commandBufferManager.reset();

    // Destroy rendering resources
    m_framebufferManager.reset();
    m_pipelineManager.reset();
    m_renderPassManager.reset();
    m_pipelineLayoutManager.reset();
    m_descriptorAllocator.reset();
    m_descriptorLayoutManager.reset();
    m_shaderModuleManager.reset();

    // Destroy render graph system
    m_renderGraphManager.reset();

    // Destroy resource managers
    m_textureManager.reset();
    m_samplerManager.reset();
    m_bufferManager.reset();
    m_swapChain.reset();
    m_attachmentManager.reset();

    // Destroy core managers
    m_vramManager.reset();
    m_syncResourceManager.reset();

    // Finally destroy the Vulkan context
    m_vulkanContext.reset();
}

void EngineCore::advanceFrame()
{
    m_frameManager->advanceFrame();
}


