#pragma once
#include "IAssetHandler.h"
#include "AssetHandle.h"
#include "Entity.h"
#include "Handle.h"
#include "Component.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <json.hpp>
#include "AssetLoader.h"

// Prefab entity data - optimized for fast instantiation
struct PrefabEntity {
    std::string name;
    Entity parent = Entity(0);
    std::vector<Entity> children;
    std::unordered_map<std::string, nlohmann::json> components;
};

// Complete prefab data structure
struct Prefab {
    Entity rootEntity;
    std::unordered_map<Entity, PrefabEntity> entities; // entity -> data
    uint32_t entityCount;
};

class PrefabManager : public IAssetHandler {
public:
    PrefabManager();
    ~PrefabManager() override = default;

    // IAssetHandler interface
    bool prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) override;
    void unloadAsset(const std::string& filename) override;
    bool isAssetReady(const std::string& filename) const override;
    uint64_t getAssetSize(const std::string& filename) const override;
    bool isInVram() const override { return false; }
    std::vector<AssetDependency> getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const override;
    std::any getResourceInternal(const AssetHandle& handle) const override;
    std::any getHandleInternal(const std::string& filename) const override;

    // Prefab-specific methods
    const Prefab* getPrefab(PrefabHandle handle) const;
    PrefabHandle createPrefab(const Prefab& prefabData, const std::string& filename);

    // Prefab persistence
    bool savePrefabToFile(PrefabHandle handle);

private:
    // Maps filenames to prefab handles
    std::unordered_map<std::string, PrefabHandle> m_prefabHandles;

    // Maps handle IDs to prefabs
    std::unordered_map<uint32_t, Prefab> m_prefabs;

    // Counter for generating unique handle IDs
    uint32_t m_nextHandleId = 1;

    // Serialization helpers
    nlohmann::json serializePrefab(const Prefab& prefab) const;
    Prefab deserializePrefab(const nlohmann::json& data) const;

    // Helper methods
    std::string getPrefabFilePath(const std::string& filename) const;
};