#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>
#include <AssetLib.h>

class Editor;

namespace fs = std::filesystem;

using namespace AssetLib;

class AssetWatcher {
public:
    AssetWatcher(const std::string& sourceDir, const std::string& destDir, Editor& editor);

    void Run();

private:
    void ProcessFile(const fs::path& sourcePath);
    bool GetAssetType(const std::string& extension, AssetType& type) const;
    fs::path GetDestinationPath(const fs::path& sourcePath, AssetType type) const;
    bool NeedsConversion(const fs::path& sourcePath, const fs::path& destPath) const;
    std::string GetSourceFromMetadata(const fs::path& destPath) const;

    fs::path sourceDirectory;
    fs::path destinationDirectory;
    std::unordered_map<std::string, AssetType> extensionMap;
    Editor& m_editor;
};