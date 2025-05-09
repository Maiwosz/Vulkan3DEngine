#include "GlobalStateManager.h"
#include "Registry.h"
#include "Renderer.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include <spdlog/spdlog.h>
#include <UBOStandardDefinitions.h>
#include "DescriptorWriter.h"
#include "DescriptorLayoutManager.h"
#include "LogicalDevice.h"

GlobalStateManager::GlobalStateManager(Registry& registry, Renderer& renderer)
    : m_registry(registry),
    m_uniformBufferManager(renderer.uniformBufferManager()),
    m_descriptorAllocator(renderer.descriptorAllocator()),
    m_descriptorLayoutManager(renderer.descriptorLayoutManager()),
    m_device(renderer.vulkanContext().logical())
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
    SPDLOG_DEBUG("Added light to frame, entity: {}, type: {}",
        light->entity.id,
        light->lightType == LightRenderOrder::LightType::Directional ? "Directional" : "Point");
}

// Create global uniform buffer with camera and light data
UniformBufferHandle GlobalStateManager::createGlobalUniformBuffer() {
    if (!m_activeCamera) {
        SPDLOG_WARN("No active camera available to create global uniform buffer");
        return UniformBufferHandle();
    }

    // Get entity references from render orders
    Entity cameraEntity = m_activeCamera->entity;

    // Get components from registry
    auto& cameraComponent = m_registry.getComponent<CameraComponent>(cameraEntity);
    auto& cameraTransform = m_registry.getComponent<TransformComponent>(cameraEntity);

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
        if (lightOrder->lightType == LightRenderOrder::LightType::Directional && !hasDirectionalLight) {
            Entity lightEntity = lightOrder->entity;
            auto& lightComponent = m_registry.getComponent<LightComponent>(lightEntity);
            auto& lightTransform = m_registry.getComponent<TransformComponent>(lightEntity);

            // Get light direction from component
            globalUboData.directionalLight.direction = glm::normalize(lightComponent.direction);
            globalUboData.directionalLight.color = lightComponent.color;

            hasDirectionalLight = true;
            SPDLOG_DEBUG("Added directional light to global UBO, entity: {}", lightEntity.id);
        }
    }

    // Process point lights
    for (auto& lightOrder : m_lights) {
        if (lightOrder->lightType == LightRenderOrder::LightType::Point && pointLightCount < 64) {
            Entity lightEntity = lightOrder->entity;
            auto& lightComponent = m_registry.getComponent<LightComponent>(lightEntity);
            auto& lightTransform = m_registry.getComponent<TransformComponent>(lightEntity);

            // Get light position from transform
            globalUboData.pointLights[pointLightCount].position = lightTransform.getPosition();
            globalUboData.pointLights[pointLightCount].radius = lightComponent.radius;
            globalUboData.pointLights[pointLightCount].color = lightComponent.color;

            pointLightCount++;
            SPDLOG_DEBUG("Added point light to global UBO, entity: {}", lightEntity.id);
        }
    }

    // Set active light counts
    globalUboData.activePointLights = pointLightCount;
    globalUboData.activeSpotLights = spotLightCount;

    // Create and update uniform buffer
    UniformBufferHandle globalUboHandle = m_uniformBufferManager.createBuffer(ShaderLib::GLOBAL_UBO);
    m_uniformBufferManager.updateBuffer(globalUboHandle, &globalUboData, sizeof(ShaderLib::GlobalUBOData));

    SPDLOG_DEBUG("Created global uniform buffer with {} point lights", pointLightCount);

    // Store the handle for later use
    m_globalUBO = globalUboHandle;
    return globalUboHandle;
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

    // Create uniform buffer and descriptor set
    createGlobalUniformBuffer();

    m_globalDataBuilt = true;
    SPDLOG_DEBUG("Global data built successfully");
}

void GlobalStateManager::applyGlobalDataToMesh(std::shared_ptr<MeshRenderOrder> mesh) {
    if (!m_globalDataBuilt) {
        SPDLOG_WARN("Attempting to apply global data before it's built");
        buildGlobalData();
    }

    // Set global UBO and descriptor set on the mesh
    mesh->globalUBOHandle = m_globalUBO;

    SPDLOG_DEBUG("Applied global data to mesh, entity: {}", mesh->entity.id);
}

void GlobalStateManager::reset() {
    m_activeCamera = nullptr;
    m_lights.clear();
    m_globalUBO = UniformBufferHandle(); // reset to invalid
    m_globalDataBuilt = false;

    SPDLOG_DEBUG("Global state manager reset for next frame");
}