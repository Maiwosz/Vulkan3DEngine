#include "MeshManager.h"
#include <stdexcept>
#include <spdlog/spdlog.h>

MeshManager::MeshManager(VramManager& vramManager)
    : m_vramManager(vramManager) {
}

MeshManager::~MeshManager() {
    // Clean up all allocated resources
    for (const auto& [handleId, mesh] : m_meshes) {
        m_vramManager.freeResource(mesh.vertexBuffer);
        m_vramManager.freeResource(mesh.indexBuffer);
    }
    m_meshes.clear();
    m_meshHandles.clear();
}

bool MeshManager::prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) {
    try {
        // Make sure this is actually a mesh asset
        if (handle.type != AssetType::Mesh) {
            SPDLOG_ERROR("Attempted to prepare non-mesh asset with MeshManager: {}", handle.filename);
            return false;
        }

        // Extract mesh data from asset
        auto [meshInfo, vertexData, indexData] = AssetLib::ReadMesh(data);

        // Create mesh resource
        MeshHandle meshHandle = createMesh(meshInfo, vertexData, indexData);

        // Store the mapping between filename and handle
        m_meshHandles[handle.filename] = meshHandle;

        SPDLOG_INFO("Successfully prepared mesh asset: {}", handle.filename);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to prepare mesh asset {}: {}", handle.filename, e.what());
        return false;
    }
}

void MeshManager::unloadAsset(const std::string& filename) {
    auto it = m_meshHandles.find(filename);
    if (it != m_meshHandles.end()) {
        MeshHandle handle = it->second;

        // Find the mesh
        auto meshIt = m_meshes.find(handle.id);
        if (meshIt != m_meshes.end()) {
            // Free VRAM resources
            m_vramManager.freeResource(meshIt->second.vertexBuffer);
            m_vramManager.freeResource(meshIt->second.indexBuffer);

            // Remove mesh from maps
            m_meshes.erase(meshIt);
        }

        // Remove the filename->handle mapping
        m_meshHandles.erase(it);

        SPDLOG_INFO("Unloaded mesh asset: {}", filename);
    }
}

bool MeshManager::isAssetReady(const std::string& filename) const {
    // Check if we have a handle for this asset
    auto it = m_meshHandles.find(filename);
    if (it != m_meshHandles.end()) {
        MeshHandle handle = it->second;
        // Verify the mesh actually exists
        return m_meshes.find(handle.id) != m_meshes.end();
    }
    return false;
}

uint64_t MeshManager::getAssetSize(const std::string& filename) const {
    auto it = m_meshHandles.find(filename);
    if (it != m_meshHandles.end()) {
        MeshHandle handle = it->second;
        auto meshIt = m_meshes.find(handle.id);
        if (meshIt != m_meshes.end()) {
            const Mesh& mesh = meshIt->second;

            // Use VramManager to get the actual allocated size in VRAM
            uint64_t vertexBufferSize = m_vramManager.getResourceSize(mesh.vertexBuffer);
            uint64_t indexBufferSize = m_vramManager.getResourceSize(mesh.indexBuffer);

            return vertexBufferSize + indexBufferSize;
        }
    }
    return 0;
}

bool MeshManager::isInVram() const {
    // Mesh resources are stored in VRAM
    return true;
}

std::vector<AssetDependency> MeshManager::getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const {
    // Meshes typically don't have dependencies on other assets
    return {};
}

std::any MeshManager::getResourceInternal(const AssetHandle& handle) const {
    auto it = m_meshHandles.find(handle.filename);
    if (it != m_meshHandles.end()) {
        const Mesh* mesh = getMesh(it->second);
        if (mesh) {
            return *mesh;
        }
    }
    return {};
}

std::any MeshManager::getHandleInternal(const std::string& filename) const {
    auto it = m_meshHandles.find(filename);
    if (it != m_meshHandles.end()) {
        return it->second;
    }
    return MeshHandle{};
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

    // Create handle and store mesh
    MeshHandle handle(m_nextHandleId++);
    m_meshes[handle.id] = std::move(newMesh);

    return handle;
}

const Mesh* MeshManager::getMesh(MeshHandle handle) const {
    auto it = m_meshes.find(handle.id);
    return it != m_meshes.end() ? &it->second : nullptr;
}