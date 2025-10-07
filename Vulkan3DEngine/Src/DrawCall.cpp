#include "DrawCall.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include "RenderNode.h"
#include "EngineCore.h"
#include "PipelineManager.h"
#include "RenderPassManager.h"
#include <spdlog/spdlog.h>

DrawCall::DrawCall(const MeshData& meshData, uint32_t instanceCount)
    : m_meshData(meshData), m_instanceCount(instanceCount) {
}

DrawCall::DrawCall(uint32_t instanceCount)
    : m_instanceCount(instanceCount) {
}

bool DrawCall::execute(Renderer& renderer, EngineCore& engineCore, RenderNode& renderNode) {
    if (!renderer.isFrameActive()) {
        SPDLOG_ERROR("DrawCall: No active frame");
        return false;
    }

    if (!hasMeshData()) {
        SPDLOG_ERROR("DrawCall: No mesh data set");
        return false;
    }

    if (!hasPipelineConfig()) {
        SPDLOG_ERROR("DrawCall: No pipeline configuration set");
        return false;
    }

    if (!validateMeshData(renderer)) {
        return false;
    }

    // Get or create pipeline with render node's render pass
    PipelineHandle pipelineHandle = getOrCreatePipeline(engineCore, renderNode);
    if (!pipelineHandle.isValid()) {
        SPDLOG_ERROR("DrawCall: Failed to get pipeline");
        return false;
    }

    // Bind the pipeline
    if (!renderer.bindPipeline(pipelineHandle)) {
        SPDLOG_ERROR("DrawCall: Failed to bind pipeline");
        return false;
    }

    return executeDrawCommand(renderer);
}

PipelineHandle DrawCall::getOrCreatePipeline(EngineCore& engineCore, RenderNode& renderNode) {
    PipelineManager& pipelineManager = engineCore.pipelineManager();
    RenderPassManager& renderPassManager = engineCore.renderPassManager();

    // Get the base pipeline configuration
    GraphicsPipelineConfig config = m_pipelineConfig;

    // Get the VkRenderPass from RenderNode's render pass handle
    RenderPassHandle renderPassHandle = renderNode.getRenderPassHandle();
    VkRenderPass* renderPassResource = renderPassManager.getResource(renderPassHandle);

    if (!renderPassResource) {
        SPDLOG_ERROR("DrawCall: Invalid render pass handle from render node");
        return PipelineHandle(0);
    }

    // Complete the pipeline configuration with render pass information
    config.renderPass.renderPass = *renderPassResource;
    config.renderPass.subpass = 0; // Default to first subpass

    // Create or retrieve cached pipeline from PipelineManager
    PipelineHandle pipelineHandle = pipelineManager.createGraphicsPipeline(config);

    if (!pipelineHandle.isValid()) {
        SPDLOG_ERROR("DrawCall: Failed to create graphics pipeline");
        return PipelineHandle(0);
    }

    SPDLOG_DEBUG("DrawCall: Successfully created/retrieved pipeline");
    return pipelineHandle;
}

void DrawCall::setDescriptorSet(uint32_t slot, const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& handle)
{
    if (slot >= m_descriptorSets.size()) {
        m_descriptorSets.resize(slot + 1);
    }
    m_descriptorSets[slot] = handle;
}

const SmartHandle<DescriptorSetHandle, VkDescriptorSet>& DrawCall::getDescriptorSet(uint32_t slot) const {
    static const SmartHandle<DescriptorSetHandle, VkDescriptorSet> empty;
    return slot < m_descriptorSets.size() ? m_descriptorSets[slot] : empty;
}

bool DrawCall::validateMeshData(Renderer& renderer) const {
    if (m_meshData.vertexCount == 0 || m_meshData.indexCount == 0) {
        SPDLOG_WARN("DrawCall: Zero vertices or indices");
        return false;
    }

    if (m_instanceCount == 0) {
        SPDLOG_WARN("DrawCall: Zero instances");
        return false;
    }

    MeshRenderer& meshRenderer = renderer.meshRenderer();

    if (!meshRenderer.validateBuffer(m_meshData.vertexBuffer, "vertex")) {
        return false;
    }

    if (!meshRenderer.validateBuffer(m_meshData.indexBuffer, "index")) {
        return false;
    }

    return true;
}

bool DrawCall::executeDrawCommand(Renderer& renderer) const {
    VkCommandBuffer cmdBuffer = renderer.getCurrentCommandBuffer();
    MeshRenderer& meshRenderer = renderer.meshRenderer();

    std::vector<DescriptorSetHandle> handles;
    for (const auto& smartHandle : m_descriptorSets) {
        if (smartHandle.isValid()) {
            handles.push_back(smartHandle.handle());
        }
    }
    renderer.bindDescriptorSets(handles);

    // Bind vertex and index buffers
    if (!meshRenderer.bindBuffers(cmdBuffer, m_meshData.vertexBuffer,
        m_meshData.indexBuffer, m_meshData.indexType)) {
        return false;
    }

    // Execute draw command
    vkCmdDrawIndexed(cmdBuffer, m_meshData.indexCount, m_instanceCount, 0, 0, 0);

    // Update statistics
    meshRenderer.updateStats(m_meshData.vertexCount, m_meshData.indexCount, m_instanceCount);

    SPDLOG_DEBUG("DrawCall executed - vertices: {}, indices: {}, instances: {}",
        m_meshData.vertexCount, m_meshData.indexCount, m_instanceCount);

    return true;
}