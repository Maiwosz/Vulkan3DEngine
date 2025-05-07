#include "UniformBufferStage.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

UniformBufferStage::UniformBufferStage(Registry& registry,
    Renderer& renderer
)
    : m_registry(registry)
    , m_shaderManager(renderer.shaderModuleManager())
    , m_uniformBufferManager(renderer.uniformBufferManager())
    , m_materialManager(renderer.materialManager())
{
    SPDLOG_INFO("Initializing UniformBufferStage");

    // Initialize the global UBO
    m_globalUBO = {};
    m_globalUBO.activePointLights = 0;
    m_globalUBO.activeSpotLights = 0;

    // Create a global uniform buffer
    ShaderLib::UniformBufferObject uboInfo;
    uboInfo.name = "GlobalUBO";
    uboInfo.size = sizeof(GlobalUBO);
    uboInfo.set = 0;
    uboInfo.binding = 0;

    m_globalUBOHandle = m_uniformBufferManager.createBuffer(uboInfo);

    if (m_globalUBOHandle) {
        SPDLOG_INFO("Created global UBO: name={}, size={}, set={}, binding={}",
            uboInfo.name, uboInfo.size, uboInfo.set, uboInfo.binding);
    }
    else {
        SPDLOG_ERROR("Failed to create global UBO!");
    }
}

void UniformBufferStage::process(std::shared_ptr<RenderOrder> order)
{
    if (!order) {
        SPDLOG_WARN("Attempted to process null render order");
        return;
    }

    SPDLOG_DEBUG("Processing render order of type: {}", renderOrderTypeToString(order->getType()));

    switch (order->getType()) {
    case RenderOrderType::Camera:
        processCameraOrder(std::static_pointer_cast<CameraRenderOrder>(order));
        break;
    case RenderOrderType::Light:
        processLightOrder(std::static_pointer_cast<LightRenderOrder>(order));
        break;
    case RenderOrderType::Mesh:
        processMeshOrder(std::static_pointer_cast<MeshRenderOrder>(order));
        break;
    default:
        SPDLOG_WARN("Unknown render order type: {}", static_cast<int>(order->getType()));
        break;
    }

    // Forward the order to the next stage
    forwardToNextStage(order);
}

void UniformBufferStage::processBatch(const std::vector<std::shared_ptr<RenderOrder>>& orders)
{
    SPDLOG_INFO("Processing batch of {} render orders", orders.size());

    // Reset the global UBO state
    m_globalUBO = {};
    m_globalUBO.activePointLights = 0;
    m_globalUBO.activeSpotLights = 0;
    m_globalUBOUpdated = false;

    // First process all camera and light orders to populate the global UBO
    processCameraOrders(orders);
    processLightOrders(orders);

    // Update the global UBO to the GPU
    updateGlobalUBO();

    // Then process mesh orders which need the global UBO
    int meshOrderCount = 0;
    for (const auto& order : orders) {
        if (order->getType() == RenderOrderType::Mesh) {
            processMeshOrder(std::static_pointer_cast<MeshRenderOrder>(order));
            forwardToNextStage(order);
            meshOrderCount++;
        }
    }

    SPDLOG_DEBUG("Processed {} mesh orders with global UBO", meshOrderCount);
}

void UniformBufferStage::processCameraOrder(std::shared_ptr<CameraRenderOrder> order)
{
    Entity entity = order->entity;

    if (!m_registry.valid(entity)) {
        SPDLOG_WARN("Attempted to process camera order with invalid entity");
        return;
    }

    if (!m_registry.hasComponent<CameraComponent>(entity) || !m_registry.hasComponent<TransformComponent>(entity)) {
        SPDLOG_WARN("Entity {} missing required components for camera processing", entity.id);
        return;
    }

    auto& camera = m_registry.getComponent<CameraComponent>(entity);
    auto& transform = m_registry.getComponent<TransformComponent>(entity);

    // Calculate view matrix (inverse of camera transform)
    glm::mat4 viewMatrix = glm::inverse(transform.getModelMatrix());

    // Get projection matrix from camera component
    glm::mat4 projMatrix = camera.calculateProjectionMatrix();

    SPDLOG_DEBUG("Camera [Entity {}] at position [{:.2f}, {:.2f}, {:.2f}]",
        entity.id, transform.getPosition().x, transform.getPosition().y, transform.getPosition().z);

    // Update global UBO
    m_globalUBO.view = viewMatrix;
    m_globalUBO.proj = projMatrix;
    m_globalUBO.cameraPosition = transform.getPosition();

    m_globalUBOUpdated = true;
}

void UniformBufferStage::processLightOrder(std::shared_ptr<LightRenderOrder> order)
{
    Entity entity = order->entity;

    if (!m_registry.valid(entity)) {
        SPDLOG_WARN("Attempted to process light order with invalid entity");
        return;
    }

    if (!m_registry.hasComponent<LightComponent>(entity) || !m_registry.hasComponent<TransformComponent>(entity)) {
        SPDLOG_WARN("Entity {} missing required components for light processing", entity.id);
        return;
    }

    auto& light = m_registry.getComponent<LightComponent>(entity);
    auto& transform = m_registry.getComponent<TransformComponent>(entity);

    // Update global UBO based on light type
    if (light.type == LightComponent::Type::Directional) {
        m_globalUBO.directionalLight.direction = light.getDirection();
        m_globalUBO.directionalLight.color = light.getColor();

        SPDLOG_DEBUG("Directional light [Entity {}] direction [{:.2f}, {:.2f}, {:.2f}], color [{:.2f}, {:.2f}, {:.2f}]",
            entity.id,
            light.getDirection().x, light.getDirection().y, light.getDirection().z,
            light.getColor().r, light.getColor().g, light.getColor().b);
    }
    else if (light.type == LightComponent::Type::Point) {
        // Only add if we haven't exceeded the maximum
        if (m_globalUBO.activePointLights < MAX_POINT_LIGHTS) {
            int idx = m_globalUBO.activePointLights++;

            m_globalUBO.pointLights[idx].position = transform.getPosition();
            m_globalUBO.pointLights[idx].radius = light.getRadius();
            m_globalUBO.pointLights[idx].color = light.getColor();

            SPDLOG_DEBUG("Point light [Entity {}] at position [{:.2f}, {:.2f}, {:.2f}], radius {:.2f}, color [{:.2f}, {:.2f}, {:.2f}]",
                entity.id,
                transform.getPosition().x, transform.getPosition().y, transform.getPosition().z,
                light.getRadius(),
                light.getColor().r, light.getColor().g, light.getColor().b);
        }
        else {
            SPDLOG_WARN("Maximum point lights ({}) exceeded, ignoring light [Entity {}]",
                MAX_POINT_LIGHTS, entity.id);
        }
    }

    m_globalUBOUpdated = true;
}

void UniformBufferStage::processMeshOrder(std::shared_ptr<MeshRenderOrder> order)
{
    Entity entity = order->entity;

    if (!m_registry.valid(entity)) {
        SPDLOG_WARN("Attempted to process mesh order with invalid entity");
        return;
    }

    if (!m_registry.hasComponent<TransformComponent>(entity)) {
        SPDLOG_WARN("Entity {} missing required TransformComponent for mesh processing", entity.id);
        return;
    }

    auto& transform = m_registry.getComponent<TransformComponent>(entity);

    // Create and update object UBO
    ShaderLib::UniformBufferObject objectUboInfo;
    objectUboInfo.name = "ObjectUBO";
    objectUboInfo.size = sizeof(ObjectUBO);
    objectUboInfo.set = 1;
    objectUboInfo.binding = 0;

    UniformBufferHandle objectUBOHandle = m_uniformBufferManager.acquireBuffer(objectUboInfo);

    if (!objectUBOHandle) {
        SPDLOG_ERROR("Failed to acquire object UBO for entity {}", entity.id);
        return;
    }

    ObjectUBO objectUBO;
    objectUBO.model = transform.getModelMatrix();
    objectUBO.color = glm::vec4(1.0f); // Default white color, could be customized

    m_uniformBufferManager.updateBuffer(objectUBOHandle, &objectUBO, sizeof(ObjectUBO));

    Material* mat = m_materialManager.get(order->materialHandle);
    if (!mat) {
        SPDLOG_WARN("Invalid material handle for entity {}", entity.id);
        return;
    }

    SPDLOG_DEBUG("Processed mesh [Entity {}], material {}, position [{:.2f}, {:.2f}, {:.2f}]",
        entity.id,
        mat->name(),
        transform.getPosition().x, transform.getPosition().y, transform.getPosition().z);

    // Store UBO handles in render order for later stages
    order->objectUBOHandle = objectUBOHandle;
    order->globalUBOHandle = m_globalUBOHandle;
    order->materialUBOHandle = mat->uniformBuffer();
}

void UniformBufferStage::updateGlobalUBO()
{
    if (m_globalUBOUpdated) {
        SPDLOG_DEBUG("Updating global UBO, active point lights: {}, active spot lights: {}",
            m_globalUBO.activePointLights, m_globalUBO.activeSpotLights);

        m_uniformBufferManager.updateBuffer(
            m_globalUBOHandle,
            &m_globalUBO,
            sizeof(GlobalUBO)
        );
    }
    else {
        SPDLOG_DEBUG("Global UBO not updated, skipping buffer update");
    }
}

void UniformBufferStage::processCameraOrders(const std::vector<std::shared_ptr<RenderOrder>>& orders)
{
    int cameraCount = 0;

    for (const auto& order : orders) {
        if (order->getType() == RenderOrderType::Camera) {
            processCameraOrder(std::static_pointer_cast<CameraRenderOrder>(order));
            cameraCount++;
        }
    }

    SPDLOG_DEBUG("Processed {} camera orders", cameraCount);
}

void UniformBufferStage::processLightOrders(const std::vector<std::shared_ptr<RenderOrder>>& orders)
{
    int directionalLightCount = 0;
    int pointLightCount = 0;

    for (const auto& order : orders) {
        if (order->getType() == RenderOrderType::Light) {
            auto lightOrder = std::static_pointer_cast<LightRenderOrder>(order);
            processLightOrder(lightOrder);

            // Count light types for logging
            if (lightOrder->lightType == LightRenderOrder::LightType::Directional) {
                directionalLightCount++;
            }
            else if (lightOrder->lightType == LightRenderOrder::LightType::Point) {
                pointLightCount++;
            }
        }
    }

    SPDLOG_DEBUG("Processed {} light orders ({} directional, {} point)",
        directionalLightCount + pointLightCount, directionalLightCount, pointLightCount);
}

void UniformBufferStage::reset()
{
    SPDLOG_DEBUG("Resetting UniformBufferStage for next frame");

    // Reset the global UBO state for the next frame
    m_globalUBO = {};
    m_globalUBO.activePointLights = 0;
    m_globalUBO.activeSpotLights = 0;
    m_globalUBOUpdated = false;

    // Explicitly update the GPU buffer with the reset values
    updateGlobalUBO();
}