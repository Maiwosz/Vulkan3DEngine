#pragma once
#include "ProcessingStage.h"
#include "RenderOrder.h"
#include "ShaderModuleManager.h"
#include "UniformBufferManager.h"
#include "Registry.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "MaterialComponent.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <array>
#include <UBODefinitions.h>
#include "MaterialManager.h"
#include "Renderer.h"

// Forward declarations
class Registry;

// Maximum number of lights supported in the shader
constexpr int MAX_POINT_LIGHTS = 64;
constexpr int MAX_SPOT_LIGHTS = 16;

// Global Uniform Buffer Object that matches shader layout
struct GlobalUBO {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 cameraPosition;
    alignas(4) float padding1;
    alignas(16) ShaderLib::DirectionalLight directionalLight;
    alignas(16) std::array<ShaderLib::PointLight, MAX_POINT_LIGHTS> pointLights;
    alignas(4) int activePointLights;
    alignas(4) int activeSpotLights;
    alignas(8) float padding2;
};

// Object Uniform Buffer Object that matches shader layout
struct ObjectUBO {
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec4 color;
};

class UniformBufferStage : public OrderProcessingStage {
public:
    UniformBufferStage(Registry& registry,
        Renderer& shaderManager
    );
    ~UniformBufferStage() override = default;

    // Process a single render order
    void process(std::shared_ptr<RenderOrder> order) override;

    // Process a batch of render orders
    void processBatch(const std::vector<std::shared_ptr<RenderOrder>>& orders) override;

    // Reset the stage for the next frame
    void reset();

private:
    // References to needed systems
    Registry& m_registry;
    ShaderModuleManager& m_shaderManager;
    UniformBufferManager& m_uniformBufferManager;
    MaterialManager& m_materialManager;

    // Global UBO data
    GlobalUBO m_globalUBO;
    UniformBufferHandle m_globalUBOHandle;
    bool m_globalUBOUpdated = false;

    // Process specific order types
    void processCameraOrder(std::shared_ptr<CameraRenderOrder> order);
    void processLightOrder(std::shared_ptr<LightRenderOrder> order);
    void processMeshOrder(std::shared_ptr<MeshRenderOrder> order);

    // Update the global UBO to the GPU
    void updateGlobalUBO();

    // Process all camera orders to update the global UBO
    void processCameraOrders(const std::vector<std::shared_ptr<RenderOrder>>& orders);

    // Process all light orders to update the global UBO
    void processLightOrders(const std::vector<std::shared_ptr<RenderOrder>>& orders);
};