#pragma once
#include "VramManager.h"
#include "Handle.h"
#include <AssetLib.h>
#include <unordered_map>
#include <string>
#include "Mesh.h"

class MeshManager {
public:
    explicit MeshManager(VramManager& vramManager);
    ~MeshManager();

    MeshHandle createMesh(const AssetLib::MeshInfo& info,
        const std::vector<uint8_t>& vertexData,
        const std::vector<uint8_t>& indexData);

    // New immediate mesh creation function for critical resources
    MeshHandle createMesh(
        VramHandle vertexBuffer,
        uint32_t vertexCount,
        VramHandle indexBuffer,
        uint32_t indexCount,
        uint8_t indexType
    );

    // New immediate mesh creation function that handles the vertex/index data
    MeshHandle immediateCreateMesh(
        const void* vertexData,
        VkDeviceSize vertexBufferSize,
        VkBufferUsageFlags vertexBufferUsage,
        uint32_t vertexCount,
        const void* indexData,
        VkDeviceSize indexBufferSize,
        VkBufferUsageFlags indexBufferUsage,
        uint32_t indexCount,
        uint8_t indexType,
        uint32_t vertexStride = 0,
        uint32_t attributes = 0
    );

    const Mesh* getMesh(MeshHandle handle) const;
    void destroyMesh(MeshHandle handle);

    // For tracking usage
    void markUsed(MeshHandle handle, uint64_t frameNumber);
    void purgeUnusedMeshes(uint64_t frameThreshold);

private:
    VramManager& m_vramManager;
    std::unordered_map<uint32_t, Mesh> m_meshes;
    std::unordered_map<uint32_t, uint64_t> m_lastUsedFrame;
    uint32_t m_nextHandleId = 1; // 0 is reserved for invalid handle
};