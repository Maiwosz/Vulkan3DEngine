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
#include "ImageSamplerManager.h"
#include "AssetSystem.h"
#include "MeshRenderOrder.h"

class DescriptorSetStage : public OrderProcessingStage {
public:
    DescriptorSetStage(
        Renderer& renderer,
        AssetSystem& assetSystem
    );
    ~DescriptorSetStage() override = default;

    // Process a single render order and optionally pass it to next stages
    void process(std::shared_ptr<RenderOrder> order) override;

private:
    // Process different types of render orders
    void processMeshOrder(std::shared_ptr<MeshRenderOrder> order);

    // Create and bind descriptor sets for different shader set bindings
    void createObjectDescriptorSet(std::shared_ptr<MeshRenderOrder> order, ShaderHandle shader);

    Renderer& m_renderer;
	AssetSystem& m_assetSystem;
    ShaderManager& m_shaderManager;
    MaterialManager& m_materialManager;
    UniformBufferManager& m_uniformBufferManager;
    DescriptorAllocator& m_descriptorAllocator;
    VramManager& m_vramManager;
    DescriptorLayoutManager& m_layoutManager;
    ImageSamplerManager& m_samplerManager;
    DescriptorWriter m_writer;
};