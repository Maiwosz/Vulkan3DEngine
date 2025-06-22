#include "AssetWatcher.h"
#include "Editor.h"
#include <fstream>
#include <json.hpp>
#include <algorithm>
#include "AssetLoader.h"
#include "ConverterLib.h"
#include "LoggerManager.h"

using json = nlohmann::json;

AssetWatcher::AssetWatcher(const std::string& sourceDir, const std::string& destDir, Editor& editor)
    : m_editor(editor)
    , sourceDirectory(fs::absolute(sourceDir))
    , destinationDirectory(fs::absolute(destDir))
{
    EDITOR_LOG_DEBUG("AssetWatcher configured with:\nSource: {}\nDestination: {}",
        sourceDirectory.string(), destinationDirectory.string());

    // Log supported extensions for debugging
    auto supportedExtensions = Converter::GetAllSupportedExtensions();
    std::string extensionsStr;
    for (const auto& ext : supportedExtensions) {
        if (!extensionsStr.empty()) extensionsStr += ", ";
        extensionsStr += std::string(ext);
    }
    EDITOR_LOG_DEBUG("Supported extensions: {}", extensionsStr);
}

void AssetWatcher::Run() {
    try {
        // Check if source directory exists
        if (!fs::exists(sourceDirectory)) {
            EDITOR_LOG_ERROR("Source directory not found: {}", sourceDirectory.string());
            return;
        }

        // Ensure it's a directory
        if (!fs::is_directory(sourceDirectory)) {
            EDITOR_LOG_ERROR("Source path is not a directory: {}", sourceDirectory.string());
            return;
        }

        try {
            for (const auto& entry : fs::recursive_directory_iterator(sourceDirectory)) {
                if (!entry.is_regular_file()) continue;
                ProcessFile(entry.path());
            }
        }
        catch (const fs::filesystem_error& e) {
            EDITOR_LOG_ERROR("Filesystem error: {}", e.what());
        }
    }
    catch (const std::exception& e) {
        EDITOR_LOG_CRITICAL("Fatal error in AssetWatcher: {}", e.what());
    }
    catch (...) {
        EDITOR_LOG_CRITICAL("Unknown fatal error in AssetWatcher");
    }
}

void AssetWatcher::ProcessFile(const fs::path& sourcePath) {
    std::string extension = sourcePath.extension().string();

    // Remove the dot from extension for comparison
    if (!extension.empty() && extension[0] == '.') {
        extension = extension.substr(1);
    }

    // Convert to lowercase for case-insensitive comparison
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    // Check if extension is supported using ConverterLib
    if (!Converter::IsExtensionSupported(extension)) {
        EDITOR_LOG_DEBUG("Skipping unsupported file type: {} (extension: {})",
            sourcePath.string(), extension);
        return;
    }

    // Get asset type from extension using ConverterLib
    AssetType type = Converter::GetAssetTypeFromExtension(extension);

    fs::path destPath = GetDestinationPath(sourcePath, type);
    EDITOR_LOG_DEBUG("Checking: {} -> {}", sourcePath.string(), destPath.string());

    if (!NeedsConversion(sourcePath, destPath)) {
        EDITOR_LOG_DEBUG("Skipping conversion (up to date): {}", sourcePath.string());
        return;
    }

    if (!fs::exists(destPath.parent_path())) {
        EDITOR_LOG_DEBUG("Creating directories: {}", destPath.parent_path().string());
    }
    fs::create_directories(destPath.parent_path());

    Converter::Settings settings;

    try {
        Converter converter;
        EDITOR_LOG_DEBUG("Attempting conversion: {} -> {}", sourcePath.string(), destPath.string());
        converter.Convert(sourcePath.string(), destPath.string(), settings);
        EDITOR_LOG_INFO("Converted: {} -> {}", sourcePath.string(), destPath.string());
    }
    catch (const std::exception& e) {
        EDITOR_LOG_ERROR("Error converting {}: {}", sourcePath.string(), e.what());
    }
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
    EDITOR_LOG_TRACE("Generated dest path: {}", destPath.string());
    return destPath;
}

bool AssetWatcher::NeedsConversion(const fs::path& sourcePath, const fs::path& destPath) const {
    if (!fs::exists(destPath)) {
        EDITOR_LOG_DEBUG("Destination file missing: {}", destPath.string());
        return true;
    }

    auto sourceTime = fs::last_write_time(sourcePath);
    auto destTime = fs::last_write_time(destPath);

    if (sourceTime > destTime) {
        EDITOR_LOG_DEBUG("Source is newer than destination: {}", sourcePath.string());
        return true;
    }

    std::string storedSource = GetSourceFromMetadata(destPath);
    std::string currentSourceName = sourcePath.filename().string();

    if (storedSource != currentSourceName) {
        EDITOR_LOG_DEBUG("Source name mismatch for {} (stored: '{}', current: '{}')",
            destPath.string(), storedSource, currentSourceName);
        return true;
    }

    EDITOR_LOG_DEBUG("File up to date: {}", destPath.string());
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