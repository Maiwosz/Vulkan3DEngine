#pragma once
#include "AssetLib.h"
#include <string>
#include <memory>
#include <vector>
#include <json.hpp>
#include "AssetHandle.h"

class AssetLoader {
public:
    AssetLoader();

    AssetLib::AssetData load(const AssetHandle handle);

    static std::string GetAssetSubdirectory(AssetLib::AssetType type);
private:
    std::string m_basePath;

    std::vector<uint8_t> loadFile(const std::string& path) const;
    std::string findFile(const std::string& baseName, AssetType type) const;
};