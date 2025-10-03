#pragma once
#include "Prerequisites.h"
#include "Handle.h"
#include <vulkan/vulkan.h>

class VulkanContext;
class VramManager;
class Buffer;

/**
 * Mesh rendering service - provides utilities for mesh operations
 * Pure service class without state management
 */
class MeshRenderer {
public:
    struct Stats {
        uint32_t drawCalls = 0;
        uint32_t verticesRendered = 0;
        uint32_t trianglesRendered = 0;
        uint32_t instancesRendered = 0;
    };

    explicit MeshRenderer(VulkanContext& context, VramManager& vramManager);

    // Service methods - stateless operations
    bool bindBuffers(VkCommandBuffer cmdBuffer, VramHandle vertexBuffer, VramHandle indexBuffer, uint8_t indexType) const;
    bool validateBuffer(VramHandle bufferHandle, const char* debugName) const;
    VkIndexType getVkIndexType(uint8_t indexType) const;

    // Stats management
    void updateStats(uint32_t vertexCount, uint32_t indexCount, uint32_t instanceCount);
    const Stats& getStats() const { return m_stats; }
    void resetStats() { m_stats = {}; }

private:
    VulkanContext& m_vulkanContext;
    VramManager& m_vramManager;
    Stats m_stats;
};