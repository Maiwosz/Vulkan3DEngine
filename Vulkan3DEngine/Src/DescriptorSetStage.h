#pragma once
#include <memory>
#include "ProcessingStage.h"
#include "Registry.h"
#include "DescriptorAllocator.h"
#include "DescriptorWriter.h"
#include "ShaderModuleManager.h"
#include "MaterialManager.h"
#include "UniformBufferManager.h"
#include "VramManager.h"
#include "RenderOrder.h"
#include "Renderer.h"

class DescriptorSetStage : public OrderProcessingStage {
public:
    DescriptorSetStage(
        Renderer& renderer
    );
    ~DescriptorSetStage() override = default;

    // Process a single render order and optionally pass it to next stages
    void process(std::shared_ptr<RenderOrder> order) override;

private:
    // Process different types of render orders
    void processMeshOrder(std::shared_ptr<MeshRenderOrder> order);
    void processLightOrder(std::shared_ptr<LightRenderOrder> order);
    void processCameraOrder(std::shared_ptr<CameraRenderOrder> order);

    // Create and bind descriptor sets for different shader set bindings
    void createGlobalDescriptorSet(std::shared_ptr<MeshRenderOrder> order, ShaderHandle shader);
    void createObjectDescriptorSet(std::shared_ptr<MeshRenderOrder> order, ShaderHandle shader);
    void createMaterialDescriptorSet(std::shared_ptr<MeshRenderOrder> order, ShaderHandle shader, Material* material);

    Renderer& m_renderer;
    ShaderModuleManager& m_shaderManager;
    MaterialManager& m_materialManager;
    UniformBufferManager& m_uniformBufferManager;
    DescriptorAllocator& m_descriptorAllocator;
    VramManager& m_vramManager;
    DescriptorLayoutManager& m_layoutManager;
    DescriptorWriter m_writer;
};