#pragma once
#include "GpuCall.h"
#include "Prerequisites.h"
#include "Handle.h"
#include "PipelineConfig.h"
#include "DescriptorAllocator.h"

/**
 * GPU command for mesh geometry rendering
 * Contains all data needed for execution and implements its own logic
 */
class DrawCall : public GpuCall {
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

    bool execute(Renderer& renderer, EngineCore& engineCore) override;

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
    void setObjectDescriptorSet(const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& descriptorSet) {
        m_objectDescriptorSetHandle = descriptorSet;
    }
    void setMaterialDescriptorSet(const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& descriptorSet) {
        m_materialDescriptorSetHandle = descriptorSet;
    }
    void setGlobalDescriptorSet(const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& descriptorSet) {
        m_globalDescriptorSetHandle = descriptorSet;
    }
    const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& getObjectDescriptorSet() const {
        return m_objectDescriptorSetHandle;
    }
    const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& getMaterialDescriptorSet() const {
        return m_materialDescriptorSetHandle;
    }
    const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& getGlobalDescriptorSet() const {
        return m_globalDescriptorSetHandle;
	}

private:
    MeshData m_meshData;
    uint32_t m_instanceCount = 0;
    GraphicsPipelineConfig m_pipelineConfig = GraphicsPipelineConfig();
    bool m_hasPipelineConfig = false;
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> m_objectDescriptorSetHandle;
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> m_materialDescriptorSetHandle;
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> m_globalDescriptorSetHandle;

    // Internal validation and execution helpers
    bool validateMeshData(Renderer& renderer) const;
    bool executeDrawCommand(Renderer& renderer) const;
};