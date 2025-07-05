#include "GlobalStateManager.h"

#include <spdlog/spdlog.h>
#include <UBOStandardDefinitions.h>


GlobalStateManager::GlobalStateManager(Registry& registry, Renderer& renderer)
    : m_registry(registry),
    m_uniformBufferManager(renderer.uniformBufferManager()),
    m_descriptorAllocator(renderer.descriptorAllocator()),
    m_descriptorLayoutManager(renderer.descriptorLayoutManager()),
    m_device(renderer.vulkanContext().logical()),
    m_writer(m_device, renderer.imageSamplerManager(), m_uniformBufferManager, m_descriptorAllocator)
{
}

void GlobalStateManager::processCamera(std::shared_ptr<CameraRenderOrder> camera) {
    // Only use the first camera or the one marked as main
    if (!m_activeCamera) {
        m_activeCamera = camera;
        SPDLOG_DEBUG("Set active camera for frame, entity: {}", camera->entity.id);
    }
    else {
        SPDLOG_DEBUG("Ignoring additional camera, already have active camera");
    }
}

void GlobalStateManager::processLight(std::shared_ptr<LightRenderOrder> light) {
    m_lights.push_back(light);
    SPDLOG_DEBUG("Added light to frame, entity: {}",
        light->entity.id);
}

// Create global uniform buffer with camera and light data - zwraca SmartHandle
SmartHandle<UniformBufferHandle, Buffer> GlobalStateManager::createGlobalUniformBuffer() {
    if (!m_activeCamera) {
        SPDLOG_WARN("No active camera available to create global uniform buffer");
        return SmartHandle<UniformBufferHandle, Buffer>(); // Return invalid smart handle
    }

    // Get entity references from render orders
    Entity cameraEntity = m_activeCamera->entity;

    // Get components from registry
    auto& cameraComponent = m_registry.components().getComponent<CameraComponent>(cameraEntity);
    auto& cameraTransform = m_registry.components().getComponent<TransformComponent>(cameraEntity);

    ShaderLib::GlobalUBOData globalUboData;
    globalUboData.SetDefaults();

    // Configure view and projection matrices

    // Calculate view matrix (invert camera model matrix to get view)
    globalUboData.view = cameraTransform.getViewMatrix();

    glm::mat4 projMatrix = cameraComponent.getProjectionMatrix();

    // Configure projection matrix based on camera type
    glm::mat4 vulkanCorrection = glm::mat4(1.0f);
    vulkanCorrection[1][1] = -1.0f; // Flip Y for Vulkan coordinate system

    globalUboData.proj = vulkanCorrection * projMatrix;

    globalUboData.cameraPosition = cameraTransform.getPosition();;

    // Configure lights
    int pointLightCount = 0;
    int spotLightCount = 0;

    // Process directional lights - using only first one if available
    bool hasDirectionalLight = false;
    for (auto& lightOrder : m_lights) {
        Entity lightEntity = lightOrder->entity;
        auto& lightComponent = m_registry.components().getComponent<LightComponent>(lightEntity);
        auto& lightTransform = m_registry.components().getComponent<TransformComponent>(lightEntity);

        if (lightComponent.type == LightComponent::Type::Directional && !hasDirectionalLight) {
            // Get light direction from component
            globalUboData.directionalLight.direction = glm::normalize(lightComponent.direction);
            globalUboData.directionalLight.color = lightComponent.color;

            hasDirectionalLight = true;
            SPDLOG_DEBUG("Added directional light to global UBO, entity: {}", lightEntity.id);
        }
        else if (lightComponent.type == LightComponent::Type::Point && pointLightCount < 64) {
            // VERIFY ALL PARAMETERS ARE CORRECTLY SET
            globalUboData.pointLights[pointLightCount].position = lightTransform.getPosition();
            globalUboData.pointLights[pointLightCount].radius = lightComponent.radius;
            globalUboData.pointLights[pointLightCount].color = lightComponent.color;

            SPDLOG_INFO("Added point light to global UBO, entity: {}, position: ({}, {}, {}), radius: {}, color: ({}, {}, {}, {})",
                lightEntity.id,
                lightTransform.getPosition().x, lightTransform.getPosition().y, lightTransform.getPosition().z,
                lightComponent.radius,
                lightComponent.color.r, lightComponent.color.g, lightComponent.color.b, lightComponent.color.a);

            pointLightCount++;
        }
    }

    // Set active light counts
    globalUboData.activePointLights = pointLightCount;
    globalUboData.activeSpotLights = spotLightCount;

    // Create and update uniform buffer using SmartHandle
    auto globalUboHandle = m_uniformBufferManager.acquireSmartBuffer(ShaderLib::GLOBAL_UBO);
    if (globalUboHandle.isValid()) {
        m_uniformBufferManager.updateBuffer(globalUboHandle.handle(), &globalUboData, sizeof(ShaderLib::GlobalUBOData));
        SPDLOG_DEBUG("Created global uniform buffer with {} point lights", pointLightCount);
    }
    else {
        SPDLOG_ERROR("Failed to create global uniform buffer");
        return SmartHandle<UniformBufferHandle, Buffer>(); // Return invalid smart handle
    }

    // Store the smart handle for later use
    m_globalUBO = globalUboHandle;
    return globalUboHandle;
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> GlobalStateManager::createGlobalDescriptorSet() {
    if (!m_globalUBO.isValid()) {
        SPDLOG_WARN("Cannot create global descriptor set without valid global UBO");
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    try {
        // Get global descriptor set layout
        VkDescriptorSetLayout globalLayout = m_descriptorLayoutManager.getBuiltInVkLayout(DescriptorLayoutManager::BuiltInLayout::Global);
        if (globalLayout == VK_NULL_HANDLE) {
            SPDLOG_ERROR("Global descriptor set layout not found");
            return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
        }

        // Clear writer for new descriptor set
        m_writer.clear();

        // Write global UBO to descriptor set
        m_writer.writeUniformBuffer(0, m_globalUBO);

        // Create descriptor set using writer - this returns SmartHandle automatically
        auto descriptorSet = m_writer.createDescriptorSet(globalLayout);

        if (descriptorSet.isValid()) {
            SPDLOG_DEBUG("Created global descriptor set successfully");
        }
        else {
            SPDLOG_ERROR("Failed to create global descriptor set");
        }

        return descriptorSet;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception while creating global descriptor set: {}", e.what());
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }
}

void GlobalStateManager::buildGlobalData() {
    if (m_globalDataBuilt) {
        SPDLOG_WARN("Global data already built for this frame");
        return;
    }

    if (!m_activeCamera) {
        SPDLOG_WARN("No active camera available to build global data");
        return;
    }

    try {
        // Create uniform buffer and descriptor set using SmartHandle
        auto globalUbo = createGlobalUniformBuffer();
        if (!globalUbo.isValid()) {
            SPDLOG_ERROR("Failed to create global uniform buffer");
            return;
        }

        auto globalDescriptorSet = createGlobalDescriptorSet();
        if (!globalDescriptorSet.isValid()) {
            SPDLOG_ERROR("Failed to create global descriptor set");
            return;
        }

        // Store the smart handles
        m_globalUBO = globalUbo;
        m_globalDescriptorSet = globalDescriptorSet;

        m_globalDataBuilt = true;
        SPDLOG_DEBUG("Global data built successfully with SmartHandle");
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception while building global data: {}", e.what());
        m_globalDataBuilt = false;
    }
}

void GlobalStateManager::applyGlobalDataToMesh(std::shared_ptr<MeshRenderOrder> mesh) {
    if (!m_globalDataBuilt) {
        SPDLOG_WARN("Attempting to apply global data before it's built");
        buildGlobalData();
    }

    if (!hasValidGlobalData()) {
        SPDLOG_ERROR("Cannot apply invalid global data to mesh");
        return;
    }

    // Set global SmartHandle references on the mesh - SmartHandle automatycznie zarządza referencjami
    mesh->globalUBOHandle = m_globalUBO;
    mesh->globalDescriptorSetHandle = m_globalDescriptorSet;

    SPDLOG_DEBUG("Applied global data to mesh, entity: {}", mesh->entity.id);
}

void GlobalStateManager::reset() {
    m_activeCamera = nullptr;
    m_lights.clear();

    // SmartHandle automatycznie zwolni zasoby gdy zostaną zresetowane
    m_globalUBO.reset();
    m_globalDescriptorSet.reset();

    m_globalDataBuilt = false;

    SPDLOG_DEBUG("Global state manager reset for next frame - SmartHandle automatically released resources");
}