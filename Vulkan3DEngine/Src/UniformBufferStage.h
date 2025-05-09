#pragma once
#include "ProcessingStage.h"
#include "RenderOrder.h"
#include "ShaderModuleManager.h"
#include "UniformBufferManager.h"
#include "TransformComponent.h"
#include "MaterialComponent.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <array>
#include <UBODefinitions.h>
#include "MaterialManager.h"
#include "Renderer.h"


class UniformBufferStage : public OrderProcessingStage {
public:
    UniformBufferStage(Registry& registry, Renderer& renderer);
    ~UniformBufferStage() override = default;

    // Process a single render order
    void process(std::shared_ptr<RenderOrder> order) override;

private:
    // References to needed systems
    Registry& m_registry;
    ShaderModuleManager& m_shaderManager;
    UniformBufferManager& m_uniformBufferManager;
    MaterialManager& m_materialManager;

    // Process specific order types
    void processMeshOrder(std::shared_ptr<MeshRenderOrder> order);
};