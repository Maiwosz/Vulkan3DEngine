#pragma once
#include <memory>
#include "ProcessingStage.h"
#include "Registry.h"
#include "DescriptorAllocator.h"
#include "DescriptorWriter.h"
#include "ShaderModuleManager.h"
#include "MaterialManager.h"
#include "BufferManager.h"
#include "VramManager.h"
#include "RenderOrder.h"
#include "EngineCore.h"
#include "ImageSamplerManager.h"
#include "AssetSystem.h"
#include "MeshRenderOrder.h"
#include "CameraRenderOrder.h"
#include "DescriptorLayoutManager.h"

class DescriptorSetStage : public ProcessingStage {
public:
    DescriptorSetStage(
        ProcessingContext& context,
        EngineCore& renderer,
        AssetSystem& assetSystem
    );
    ~DescriptorSetStage() override = default;

    // Process a single render order - returns true on success, false on failure
    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

private:
    // Process different types of render orders - return true on success
    ProcessingResult processMeshOrder(std::shared_ptr<MeshRenderOrder> order);
    ProcessingResult processCameraOrder(std::shared_ptr<CameraRenderOrder> order);

    // Create descriptor sets for different shader set bindings - return true on success
    bool createObjectDescriptorSet(std::shared_ptr<MeshRenderOrder> order, ShaderHandle shader);
    bool createGlobalDescriptorSet(std::shared_ptr<CameraRenderOrder> order);

    EngineCore& m_renderer;
    AssetSystem& m_assetSystem;
    ShaderManager& m_shaderManager;
    MaterialManager& m_materialManager;
    BufferManager& m_bufferManager;
    DescriptorAllocator& m_descriptorAllocator;
    VramManager& m_vramManager;
    DescriptorLayoutManager& m_layoutManager;
    ImageSamplerManager& m_samplerManager;
    DescriptorWriter m_writer;
};