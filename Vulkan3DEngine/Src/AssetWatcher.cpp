#include "AssetWatcher.h"
#include "Editor.h"
#include <fstream>
#include <json.hpp>
#include <iostream>
#include "AssetLoader.h"
#include "Converter.h"

using json = nlohmann::json;

AssetWatcher::AssetWatcher(const std::string& sourceDir, const std::string& destDir, Editor& editor)
    : m_editor(editor) // <-- Inicjalizacja referencji
    , sourceDirectory(fs::absolute(sourceDir))
    , destinationDirectory(fs::absolute(destDir))
{
    m_editor.getLogger()->debug("AssetWatcher configured with:\nSource: {}\nDestination: {}", sourceDirectory.string(), destinationDirectory.string());
    extensionMap = {
        { ".png", AssetType::Texture },
        { ".jpg", AssetType::Texture },
        { ".tga", AssetType::Texture },
        { ".obj", AssetType::Mesh },
        { ".mat", AssetType::Material },
        { ".glsl", AssetType::Shader }
    };
}

void AssetWatcher::Run() {
    try {
        // Check if source directory exists
        if (!fs::exists(sourceDirectory)) {
            m_editor.getLogger()->error("Source directory not found: {}", sourceDirectory.string());
            return;
        }

        // Ensure it's a directory
        if (!fs::is_directory(sourceDirectory)) {
            m_editor.getLogger()->error("Source path is not a directory: {}", sourceDirectory.string());
            return;
        }

        try {
            for (const auto& entry : fs::recursive_directory_iterator(sourceDirectory)) {
                if (!entry.is_regular_file()) continue;
                ProcessFile(entry.path());
            }
        }
        catch (const fs::filesystem_error& e) {
            m_editor.getLogger()->error("Filesystem error: {}", e.what());
        }
    }
    catch (const std::exception& e) {
        m_editor.getLogger()->critical("Fatal error in AssetWatcher: {}", e.what());
    }
    catch (...) {
        m_editor.getLogger()->critical("Unknown fatal error in AssetWatcher");
    }
}

void AssetWatcher::ProcessFile(const fs::path& sourcePath) {
    std::string extension = sourcePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    AssetType type;
    if (!GetAssetType(extension, type)) {
        m_editor.getLogger()->debug("Skipping unsupported file type: {}", sourcePath.string());
        return;
    }

    fs::path destPath = GetDestinationPath(sourcePath, type);
    m_editor.getLogger()->debug("Checking: {} -> {}", sourcePath.string(), destPath.string());

    if (!NeedsConversion(sourcePath, destPath)) {
        m_editor.getLogger()->debug("Skipping conversion (up to date): {}", sourcePath.string());
        return;
    }

    if (!fs::exists(destPath.parent_path())) {
        m_editor.getLogger()->debug("Creating directories: {}", destPath.parent_path().string());
    }
    fs::create_directories(destPath.parent_path());

    Converter::Settings settings;

    try {
        Converter converter;
        m_editor.getLogger()->debug("Attempting conversion: {} -> {}", sourcePath.string(), destPath.string());
        converter.Convert(sourcePath.string(), destPath.string(), settings);
        m_editor.getLogger()->info("Converted: {} -> {}", sourcePath.string(), destPath.string());
    }
    catch (const std::exception& e) {
        m_editor.getLogger()->error("Error converting {}: {}", sourcePath.string(), e.what());
    }
}

bool AssetWatcher::GetAssetType(const std::string& extension, AssetType& type) const {
    auto it = extensionMap.find(extension);
    if (it == extensionMap.end()) {
        return false;
    }
    type = it->second;
    return true;
}

fs::path AssetWatcher::GetDestinationPath(const fs::path& sourcePath, AssetType type) const {
    const std::string baseName = sourcePath.stem().string();
    fs::path relative = fs::relative(sourcePath.parent_path(), sourceDirectory);
    std::string assetSubdirStr = AssetLoader::GetAssetSubdirectory(type);

    fs::path newRelative;
    auto it = relative.begin();
    if (it != relative.end() && it->string() == assetSubdirStr) {
        // Skip the first component if it matches the asset subdirectory
        ++it;
        for (; it != relative.end(); ++it) {
            newRelative /= *it;
        }
    }
    else {
        newRelative = relative;
    }

    fs::path destPath = destinationDirectory
        / assetSubdirStr
        / newRelative
        / baseName;

    destPath.replace_extension(AssetLib::Utilities::GetAssetExtension(type));
    m_editor.getLogger()->trace("Generated dest path: {}", destPath.string());
    return destPath;
}

bool AssetWatcher::NeedsConversion(const fs::path& sourcePath, const fs::path& destPath) const {
    if (!fs::exists(destPath)) {
        m_editor.getLogger()->debug("Destination file missing: {}", destPath.string());
        return true;
    }

    auto sourceTime = fs::last_write_time(sourcePath);
    auto destTime = fs::last_write_time(destPath);

    if (sourceTime > destTime) {
        m_editor.getLogger()->debug("Source is newer than destination: {}", sourcePath.string());
        return true;
    }

    std::string storedSource = GetSourceFromMetadata(destPath);
    std::string currentSourceName = sourcePath.filename().string(); // <-- Pobierz tylko nazwę pliku

    if (storedSource != currentSourceName) { // <-- Porównaj same nazwy plików
        m_editor.getLogger()->debug("Source name mismatch for {} (stored: '{}', current: '{}')",
            destPath.string(), storedSource, currentSourceName);
        return true;
    }

    m_editor.getLogger()->debug("File up to date: {}", destPath.string());
    return false;
}

std::string AssetWatcher::GetSourceFromMetadata(const fs::path& destPath) const {
    std::ifstream file(destPath, std::ios::binary);
    if (!file) return "";

    AssetLib::Header header;
    if (!file.read(reinterpret_cast<char*>(&header), sizeof(header))) return "";

    if (header.metadataSize == 0) return "";

    std::vector<uint8_t> metadata(header.metadataSize);
    if (!file.read(reinterpret_cast<char*>(metadata.data()), header.metadataSize)) return "";

    try {
        json meta = json::from_msgpack(metadata);
        if (meta.contains("source")) {
            return meta["source"].get<std::string>();
        }
    }
    catch (...) {}

    return "";
}