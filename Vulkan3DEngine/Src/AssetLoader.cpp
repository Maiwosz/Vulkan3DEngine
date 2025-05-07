#include "AssetLoader.h"
#include <fstream>
#include <filesystem>
#include <json.hpp>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

AssetLoader::AssetLoader(const std::string& basePath)
    : m_basePath(basePath) {
    SPDLOG_INFO("AssetLoader initialized with base path: {}", m_basePath);
}

AssetLib::AssetData AssetLoader::load(const AssetHandle handle) {
    SPDLOG_DEBUG("Loading asset with filename: {}, type: {}",
        handle.filename, static_cast<int>(handle.type));

    try {
        const auto path = findFile(handle.filename, handle.type);
        SPDLOG_DEBUG("Asset file found at: {}", path);

        const auto data = loadFile(path);
        if (data.size() < sizeof(AssetLib::Header)) {
            SPDLOG_ERROR("File too small to be valid: {}", path);
            throw std::runtime_error("File too small to be valid");
        }

        SPDLOG_DEBUG("Reading asset data from path: {}", path);
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
    SPDLOG_DEBUG("Loading file : {}", path);

    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            SPDLOG_ERROR("Failed to open file: {}", path);
            throw std::runtime_error("Failed to open file: " + path);
        }

        const size_t fileSize = file.tellg();
        SPDLOG_DEBUG("File size: {} bytes", fileSize);

        file.seekg(0);
        std::vector<uint8_t> buffer(fileSize);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

        SPDLOG_DEBUG("Successfully read {} bytes from file: {}", fileSize, path);
        return buffer;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error in loadFile for {}: {}", path, e.what());
        throw;
    }
}

std::string AssetLoader::findFile(const std::string& baseName, AssetType type) const {
    using namespace AssetLib::Utilities;
    SPDLOG_DEBUG("Finding file for asset: {}, type: {}",
        baseName, static_cast<int>(type));

    // Konwertuj string_view na string
    const std::string ext(GetAssetExtension(type));
    const std::string subdir(GetAssetSubdirectory(type));

    // 1. Sprawdź w domyślnym folderze dla typu
    const fs::path defaultPath = fs::path(m_basePath) / subdir / (baseName + ext);
    SPDLOG_DEBUG("Checking default path: {}", defaultPath.string());

    if (fs::exists(defaultPath)) {
        SPDLOG_DEBUG("Asset found at default path: {}", defaultPath.string());
        return defaultPath.string();
    }

    // 2. Przeszukaj rekurencyjnie cały katalog bazowy
    SPDLOG_INFO("Asset not found at default path, searching recursively in: {}", m_basePath);

    const std::string targetName = baseName + ext;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(m_basePath)) {
            if (entry.is_regular_file() && entry.path().filename() == targetName) {
                SPDLOG_INFO("Asset found at alternate location: {}", entry.path().string());
                return entry.path().string();
            }
        }

        SPDLOG_WARN("Asset not found: {}{}", baseName, ext);
        throw std::runtime_error("Asset not found: " + targetName);
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
    default:
        result = "Unknown";
        SPDLOG_WARN("Unknown asset type: {}", static_cast<int>(type));
        break;
    }
    return result;
}