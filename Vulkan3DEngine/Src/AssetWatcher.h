#pragma once
#include <string>
#include <filesystem>
#include <unordered_map>
#include "AssetLib.h"

namespace fs = std::filesystem;

class Editor;

class AssetWatcher {
public:
    using AssetType = AssetLib::AssetType;

    AssetWatcher() = delete;

    AssetWatcher(const std::string& sourceDir, const std::string& destDir, Editor& editor);
    void Run();

private:
    Editor& m_editor;
    fs::path sourceDirectory;
    fs::path destinationDirectory;
    std::unordered_map<std::string, AssetType> extensionMap;

    void ProcessFile(const fs::path& sourcePath);
    bool GetAssetType(const std::string& extension, AssetType& type) const;
    fs::path GetDestinationPath(const fs::path& sourcePath, AssetType type) const;
    bool NeedsConversion(const fs::path& sourcePath, const fs::path& destPath) const;
    std::string GetSourceFromMetadata(const fs::path& destPath) const;
};