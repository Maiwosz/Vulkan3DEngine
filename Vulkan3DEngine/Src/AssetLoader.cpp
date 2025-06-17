#include "AssetLoader.h"
#include <fstream>
#include <filesystem>
#include <json.hpp>
#include <spdlog/spdlog.h>
#include "Paths.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

AssetLoader::AssetLoader()
    : m_basePath(ASSETS_COMP) {
    SPDLOG_INFO("AssetLoader initialized with base path: {}", ASSETS_COMP);
}

AssetLib::AssetData AssetLoader::load(const AssetHandle handle) {
    try {
        const auto path = findFile(handle.filename, handle.type);
        const auto data = loadFile(path);
        if (data.size() < sizeof(AssetLib::Header)) {
            SPDLOG_ERROR("File too small to be valid: {}", path);
            throw std::runtime_error("File too small to be valid");
        }

        AssetLib::AssetData result = AssetLib::ReadAsset(path);
        SPDLOG_INFO("Successfully loaded asset: {}", handle.filename);
        return result;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to load asset {}: {}", handle.filename, e.what());
        throw;
    }
}

std::vector<uint8_t> AssetLoader::loadFile(const std::string& path) const {
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            SPDLOG_ERROR("Failed to open file: {}", path);
            throw std::runtime_error("Failed to open file: " + path);
        }

        const size_t fileSize = file.tellg();
        file.seekg(0);
        std::vector<uint8_t> buffer(fileSize);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

        return buffer;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error in loadFile for {}: {}", path, e.what());
        throw;
    }
}

std::string AssetLoader::findFile(const std::string& baseName, AssetType type) const {
    using namespace AssetLib::Utilities;

    // Check if baseName already has an extension
    fs::path baseNamePath(baseName);
    std::string providedExt = baseNamePath.has_extension() ? baseNamePath.extension().string() : "";
    std::string nameWithoutExt = baseNamePath.stem().string();

    // Get expected extension for this asset type
    const std::string expectedExt(GetAssetExtension(type));

    // Verify extension matches asset type if provided
    if (!providedExt.empty() && providedExt != expectedExt) {
        SPDLOG_WARN("Provided extension {} doesn't match expected extension {} for asset type {}",
            providedExt, expectedExt, static_cast<int>(type));
    }

    // Use provided extension if it exists, otherwise use the expected one
    const std::string finalExt = providedExt.empty() ? expectedExt : providedExt;
    const std::string subdir(GetAssetSubdirectory(type));

    // Determine if path is absolute
    bool isAbsolutePath = fs::path(baseName).is_absolute();
    std::vector<std::string> pathsToTry;

    if (isAbsolutePath) {
        // If absolute path, just use it directly
        pathsToTry.push_back(baseName);
    }
    else {
        // 1. Try in the default subdirectory for this asset type
        pathsToTry.push_back((fs::path(m_basePath) / subdir / (nameWithoutExt + finalExt)).string());

        // 2. Try in base path directly
        pathsToTry.push_back((fs::path(m_basePath) / (nameWithoutExt + finalExt)).string());

        // 3. Try the path as provided (relative to current working directory)
        pathsToTry.push_back(baseName);
    }

    // Try each path
    for (const auto& path : pathsToTry) {
        if (fs::exists(path)) {
            SPDLOG_INFO("Asset found at: {}", path);
            return path;
        }
    }

    // If none of the direct paths worked, search recursively as a last resort
    SPDLOG_INFO("Asset not found at expected paths, searching recursively in: {}", m_basePath);

    const std::string targetName = nameWithoutExt + finalExt;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(m_basePath)) {
            if (entry.is_regular_file() && entry.path().filename() == targetName) {
                SPDLOG_INFO("Asset found at alternate location: {}", entry.path().string());
                return entry.path().string();
            }
        }

        SPDLOG_WARN("Asset not found: {}{}", nameWithoutExt, finalExt);
        throw std::runtime_error("Asset not found: " + nameWithoutExt + finalExt);
    }
    catch (const fs::filesystem_error& e) {
        SPDLOG_ERROR("Filesystem error while searching for {}: {}", targetName, e.what());
        throw std::runtime_error("Filesystem error: " + std::string(e.what()));
    }
}

std::string AssetLoader::GetAssetSubdirectory(AssetLib::AssetType type) {
    std::string result;

    switch (type) {
    case AssetType::Texture:
        result = "Textures";
        break;
    case AssetType::Mesh:
        result = "Meshes";
        break;
    case AssetType::Material:
        result = "Materials";
        break;
    case AssetType::Shader:
        result = "Shaders";
        break;
    case AssetType::Prefab:
        result = "Prefabs";
        break;
    default:
        result = "Unknown";
        SPDLOG_WARN("Unknown asset type: {}", static_cast<int>(type));
        break;
    }
    return result;
}