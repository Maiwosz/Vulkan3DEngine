#pragma once
#include "IAssetHandler.h"
#include "AssetHandle.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <json.hpp>

class SceneManager : public IAssetHandler {
public:
    explicit SceneManager();
    ~SceneManager() override = default;

    // IAssetHandler interface
    bool prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) override;
    void unloadAsset(const std::string& filename) override;
    bool isAssetReady(const std::string& filename) const override;
    uint64_t getAssetSize(const std::string& filename) const override;
    bool isInVram() const override { return false; }
    std::vector<AssetDependency> getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const override;
    std::any getResourceInternal(const AssetHandle& handle) const override;
    std::any getHandleInternal(const std::string& filename) const override;

    // Scene data access
    nlohmann::json getSceneData(const std::string& filename) const;
    bool hasSceneData(const std::string& filename) const;

    // Scene file operations
    bool saveSceneToFile(const std::string& filename, const nlohmann::json& sceneData);

private:
    // Scene data storage (filename -> JSON data)
    std::unordered_map<std::string, nlohmann::json> m_loadedScenes;

    // Helper methods
    std::string getSceneFilePath(const std::string& filename) const;
};