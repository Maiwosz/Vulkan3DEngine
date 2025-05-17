#pragma once
#include "IAssetHandler.h"
#include "VramManager.h"
#include "Handle.h"
#include "Mesh.h"
#include <AssetLib.h>
#include <unordered_map>
#include <string>

class MeshManager : public IAssetHandler {
public:
    explicit MeshManager(VramManager& vramManager);
    ~MeshManager() override;

    // IAssetHandler implementation
    bool prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) override;
    void unloadAsset(const std::string& filename) override;
    bool isAssetReady(const std::string& filename) const override;
    uint64_t getAssetSize(const std::string& filename) const override;
    bool isInVram() const override;
    std::vector<AssetDependency> getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const override;
    std::any getResourceInternal(const AssetHandle& handle) const override;
    std::any getHandleInternal(const std::string& filename) const override;

    // Helper to get a mesh from its handle
    const Mesh* getMesh(MeshHandle handle) const;
private:
    VramManager& m_vramManager;

    // Maps filenames to mesh handles
    std::unordered_map<std::string, MeshHandle> m_meshHandles;

    // Maps handle IDs to meshes
    std::unordered_map<uint32_t, Mesh> m_meshes;

    // Counter for generating unique handle IDs
    uint32_t m_nextHandleId = 1; // 0 is reserved for invalid handle

    // Internal mesh creation method
    MeshHandle createMesh(
        const AssetLib::MeshInfo& info,
        const std::vector<uint8_t>& vertexData,
        const std::vector<uint8_t>& indexData
    );
};