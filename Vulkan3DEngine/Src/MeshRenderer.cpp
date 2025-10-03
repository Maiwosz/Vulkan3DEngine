#include "MeshRenderer.h"
#include "VulkanContext.h"
#include "VramManager.h"
#include "Buffer.h"
#include <spdlog/spdlog.h>

MeshRenderer::MeshRenderer(VulkanContext& context, VramManager& vramManager)
    : m_vulkanContext(context), m_vramManager(vramManager) {
    SPDLOG_DEBUG("MeshRenderer initialized as service");
}

bool MeshRenderer::bindBuffers(VkCommandBuffer cmdBuffer, VramHandle vertexBuffer,
    VramHandle indexBuffer, uint8_t indexType) const {
    auto* vbuffer = m_vramManager.getResource<Buffer>(vertexBuffer);
    auto* ibuffer = m_vramManager.getResource<Buffer>(indexBuffer);

    if (!vbuffer || !ibuffer) {
        SPDLOG_ERROR("MeshRenderer: Failed to get buffer resources");
        return false;
    }

    VkBuffer vkVertexBuffer = vbuffer->get();
    VkBuffer vkIndexBuffer = ibuffer->get();
    VkDeviceSize offset = 0;

    // Bind vertex buffer
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vkVertexBuffer, &offset);

    // Bind index buffer
    vkCmdBindIndexBuffer(cmdBuffer, vkIndexBuffer, 0, getVkIndexType(indexType));

    return true;
}

bool MeshRenderer::validateBuffer(VramHandle bufferHandle, const char* debugName) const {
    if (!bufferHandle.isValid()) {
        SPDLOG_ERROR("MeshRenderer: Invalid {} buffer handle", debugName);
        return false;
    }

    auto* buffer = m_vramManager.getResource<Buffer>(bufferHandle);
    if (!buffer || buffer->get() == VK_NULL_HANDLE) {
        SPDLOG_ERROR("MeshRenderer: Invalid {} buffer resource", debugName);
        return false;
    }

    return true;
}

VkIndexType MeshRenderer::getVkIndexType(uint8_t indexType) const {
    return indexType == 0 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
}

void MeshRenderer::updateStats(uint32_t vertexCount, uint32_t indexCount, uint32_t instanceCount) {
    m_stats.drawCalls++;
    m_stats.verticesRendered += vertexCount * instanceCount;
    m_stats.trianglesRendered += (indexCount / 3) * instanceCount;
    m_stats.instancesRendered += instanceCount;
}