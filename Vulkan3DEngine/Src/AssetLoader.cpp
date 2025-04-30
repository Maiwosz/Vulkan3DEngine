#include "AssetLoader.h"
#include <fstream>
#include <filesystem>
#include <json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

AssetLoader::AssetLoader(const std::string& basePath)
    : m_basePath(basePath) {
}

AssetLib::AssetData AssetLoader::load(const AssetHandle handle) {
    const auto path = findFile(handle.filename, handle.type);
    const auto data = loadFile(path, handle.type);

    if (data.size() < sizeof(AssetLib::Header)) {
        throw std::runtime_error("File too small to be valid");
    }

    AssetLib::AssetData result = AssetLib::ReadAsset(path);;

    return result;
}

std::vector<uint8_t> AssetLoader::loadFile(const std::string& baseName, AssetType type) const {
    const auto path = findFile(baseName, type);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    const size_t fileSize = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    return buffer;
}

std::string AssetLoader::findFile(const std::string& baseName, AssetType type) const {
    using namespace AssetLib::Utilities;

    // Konwertuj string_view na string
    const std::string ext(GetAssetExtension(type));
    const std::string subdir(GetAssetSubdirectory(type));

    // 1. Sprawdź w domyślnym folderze dla typu
    const fs::path defaultPath = fs::path(m_basePath)
        / subdir
        / (baseName + ext);

    if (fs::exists(defaultPath)) {
        return defaultPath.string();
    }

    // 2. Przeszukaj rekurencyjnie cały katalog bazowy
    const std::string targetName = baseName + ext;
    for (const auto& entry : fs::recursive_directory_iterator(m_basePath)) {
        if (entry.is_regular_file() && entry.path().filename() == targetName) {
            return entry.path().string();
        }
    }

    throw std::runtime_error("Asset not found: " + targetName);
}

std::string AssetLoader::GetAssetSubdirectory(AssetLib::AssetType type) {
    switch (type) {
    case AssetType::Texture: return "Textures";
    case AssetType::Mesh: return "Meshes";
    case AssetType::Material: return "Materials";
    case AssetType::Shader: return "Shaders";
    default: return "Unknown";
    }
}