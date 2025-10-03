#include "DrawCall.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include <spdlog/spdlog.h>

DrawCall::DrawCall(const MeshData& meshData, uint32_t instanceCount)
    : m_meshData(meshData), m_instanceCount(instanceCount) {
}

DrawCall::DrawCall(uint32_t instanceCount)
    : m_instanceCount(instanceCount) {
}

bool DrawCall::execute(Renderer& renderer, EngineCore& engineCore) {
    if (!renderer.isFrameActive()) {
        SPDLOG_ERROR("DrawCall: No active frame");
        return false;
    }

    if (!renderer.isPipelineBound()) {
        SPDLOG_ERROR("DrawCall: No pipeline bound");
        return false;
    }

    if (!hasMeshData()) {
        SPDLOG_ERROR("DrawCall: No mesh data set");
        return false;
    }

    if (!validateMeshData(renderer)) {
        return false;
    }

    return executeDrawCommand(renderer);
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

    renderer.bindDescriptorSets(std::vector<DescriptorSetHandle>{
        m_globalDescriptorSetHandle.handle(),
        m_objectDescriptorSetHandle.handle(),
        m_materialDescriptorSetHandle.handle()
    });

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