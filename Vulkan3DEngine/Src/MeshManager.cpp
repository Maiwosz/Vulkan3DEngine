#include "MeshManager.h"
#include <stdexcept>

MeshManager::MeshManager(VramManager& vramManager)
    : m_vramManager(vramManager) {
}

MeshManager::~MeshManager() {
    for (const auto& [handleId, mesh] : m_meshes) {
        m_vramManager.freeResource(mesh.vertexBuffer);
        m_vramManager.freeResource(mesh.indexBuffer);
    }
    m_meshes.clear();
}

MeshHandle MeshManager::createMesh(const AssetLib::MeshInfo& info,
    const std::vector<uint8_t>& vertexData,
    const std::vector<uint8_t>& indexData) {
    Graphics::BufferCreateInfo vertexBufferInfo{
        .usage = Graphics::BufferUsageType::Vertex,
        .size = vertexData.size()
    };

    Graphics::BufferCreateInfo indexBufferInfo{
        .usage = Graphics::BufferUsageType::Index,
        .size = indexData.size()
    };

    Mesh newMesh;
    newMesh.vertexBuffer = m_vramManager.createBuffer(
        vertexBufferInfo,
        vertexData.data()
    );

    newMesh.indexBuffer = m_vramManager.createBuffer(
        indexBufferInfo,
        indexData.data()
    );

    // Store mesh information
    newMesh.vertexCount = info.vertexCount;
    newMesh.indexCount = info.indexCount;
    newMesh.vertexStride = info.vertexStride;
    newMesh.attributes = info.attributes;
    newMesh.indexType = info.indexType;

    MeshHandle handle(m_nextHandleId++);
    m_meshes[handle.id] = std::move(newMesh);
    return handle;
}

MeshHandle MeshManager::createMesh(
    VramHandle vertexBuffer,
    uint32_t vertexCount,
    VramHandle indexBuffer,
    uint32_t indexCount,
    uint8_t indexType
) {
    Mesh newMesh;
    newMesh.vertexBuffer = vertexBuffer;
    newMesh.indexBuffer = indexBuffer;
    newMesh.vertexCount = vertexCount;
    newMesh.indexCount = indexCount;
    newMesh.indexType = indexType;

    // Default values for other fields
    newMesh.vertexStride = 0;
    newMesh.attributes = 0;

    MeshHandle handle(m_nextHandleId++);
    m_meshes[handle.id] = std::move(newMesh);
    return handle;
}

MeshHandle MeshManager::immediateCreateMesh(
    const void* vertexData,
    VkDeviceSize vertexBufferSize,
    VkBufferUsageFlags vertexBufferUsage,
    uint32_t vertexCount,
    const void* indexData,
    VkDeviceSize indexBufferSize,
    VkBufferUsageFlags indexBufferUsage,
    uint32_t indexCount,
    uint8_t indexType,
    uint32_t vertexStride,
    uint32_t attributes
) {
    // Create buffers immediately using immediate buffer creation
    VramHandle vertexBuffer = m_vramManager.createBuffer(
        vertexBufferSize,
        vertexBufferUsage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexData
    );

    VramHandle indexBuffer = m_vramManager.createBuffer(
        indexBufferSize,
        indexBufferUsage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexData
    );

    // Create mesh structure
    Mesh newMesh;
    newMesh.vertexBuffer = vertexBuffer;
    newMesh.indexBuffer = indexBuffer;
    newMesh.vertexCount = vertexCount;
    newMesh.indexCount = indexCount;
    newMesh.vertexStride = vertexStride;
    newMesh.attributes = attributes;
    newMesh.indexType = indexType;

    // Create and return handle
    MeshHandle handle(m_nextHandleId++);
    m_meshes[handle.id] = std::move(newMesh);
    return handle;
}

const Mesh* MeshManager::getMesh(MeshHandle handle) const {
    auto it = m_meshes.find(handle.id);
    return it != m_meshes.end() ? &it->second : nullptr;
}

void MeshManager::destroyMesh(MeshHandle handle) {
    auto it = m_meshes.find(handle.id);
    if (it != m_meshes.end()) {
        m_vramManager.freeResource(it->second.vertexBuffer);
        m_vramManager.freeResource(it->second.indexBuffer);
        m_meshes.erase(it);
        m_lastUsedFrame.erase(handle.id);
    }
}

void MeshManager::markUsed(MeshHandle handle, uint64_t frameNumber) {
    if (m_meshes.find(handle.id) != m_meshes.end()) {
        m_lastUsedFrame[handle.id] = frameNumber;
    }
}

void MeshManager::purgeUnusedMeshes(uint64_t frameThreshold) {
    std::vector<uint32_t> toRemove;
    for (const auto& [handleId, lastUsed] : m_lastUsedFrame) {
        if (frameThreshold > lastUsed) {
            toRemove.push_back(handleId);
        }
    }

    for (uint32_t handleId : toRemove) {
        destroyMesh(MeshHandle(handleId));
    }
}