#pragma once
#include "GpuCall.h"
#include "GpuCallTypes.h"
#include "Prerequisites.h"
#include "Handle.h"
#include "PipelineConfig.h"
#include "DescriptorAllocator.h"

// Forward declarations
class RenderNode;
class EngineCore;

/**
 * GPU command for mesh geometry rendering
 * Contains all data needed for execution and implements its own logic
 * including pipeline management based on render node context
 */
class DrawCall : public TypedGpuCall<DrawCall> {
public:
    struct MeshData {
        VramHandle vertexBuffer = VramHandle();
        VramHandle indexBuffer = VramHandle();
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint8_t indexType = 0;  // 0 = 16-bit, 1 = 32-bit
    };

    explicit DrawCall(const MeshData& meshData, uint32_t instanceCount = 1);

    // Constructor for empty DrawCall (to be filled later)
    DrawCall(uint32_t instanceCount = 1);

    bool execute(Renderer& renderer, EngineCore& engineCore, RenderNode& renderNode) override;

    const MeshData& getMeshData() const { return m_meshData; }
    uint32_t getInstanceCount() const { return m_instanceCount; }

    // Set mesh data for this draw call
    void setMeshData(const MeshData& meshData) { m_meshData = meshData; }

    // Set pipeline configuration for this draw call
    void setPipelineConfig(const GraphicsPipelineConfig& config) {
        m_pipelineConfig = config;
        m_hasPipelineConfig = true;
    }

    // Get pipeline configuration
    const GraphicsPipelineConfig& getPipelineConfig() const { return m_pipelineConfig; }

    // Check if pipeline config is set
    bool hasPipelineConfig() const { return m_hasPipelineConfig; }

    // Check if mesh data is set
    bool hasMeshData() const {
        return m_meshData.vertexBuffer.isValid() &&
            m_meshData.indexBuffer.isValid() &&
            m_meshData.vertexCount > 0 &&
            m_meshData.indexCount > 0;
    }

    // Descriptor set management
    void setDescriptorSet(uint32_t slot, const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& handle);
    const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& getDescriptorSet(uint32_t slot) const;

    void setGlobalDescriptorSet(const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& ds) {
        setDescriptorSet(ShaderLib::GLOBAL_DESCRIPTOR_SET, ds);
    }
    void setObjectDescriptorSet(const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& ds) {
        setDescriptorSet(ShaderLib::OBJECT_DESCRIPTOR_SET, ds);
    }
    void setCustomDescriptorSet(const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& ds) {
        setDescriptorSet(ShaderLib::CUSTOM_DESCRIPTOR_SET, ds);
    }

private:
    MeshData m_meshData;
    uint32_t m_instanceCount = 0;
    GraphicsPipelineConfig m_pipelineConfig = GraphicsPipelineConfig();
    bool m_hasPipelineConfig = false;
    std::vector<SmartHandle<DescriptorSetHandle, VkDescriptorSet>> m_descriptorSets;

    // Internal validation and execution helpers
    bool validateMeshData(Renderer& renderer) const;
    bool executeDrawCommand(Renderer& renderer) const;

    // Pipeline management (moved from RenderGraphExecutor)
    PipelineHandle getOrCreatePipeline(EngineCore& engineCore, RenderNode& renderNode);
};