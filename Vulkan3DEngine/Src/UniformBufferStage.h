#pragma once
#include "ProcessingStage.h"
#include "RenderOrder.h"
#include "AssetSystem.h"
#include "ShaderManager.h"
#include "BufferManager.h"
#include "TransformComponent.h"
#include "MaterialComponent.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <array>
#include "MaterialManager.h"
#include "EngineCore.h"
#include "MeshRenderOrder.h"
#include "Registry.h"

class Registry;

class UniformBufferStage : public ProcessingStage {
public:
    UniformBufferStage(ProcessingContext& context, Registry& registry, EngineCore& renderer, AssetSystem& assetSystem);
    ~UniformBufferStage() override = default;

    // Process a single render order
    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

private:
    // References to needed systems
    Registry& m_registry;
    ShaderManager& m_shaderManager;
    BufferManager& m_bufferManager;
    MaterialManager& m_materialManager;

    // Process specific order types
    ProcessingResult processMeshOrder(std::shared_ptr<MeshRenderOrder> order);
};