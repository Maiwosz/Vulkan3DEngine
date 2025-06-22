#include "SceneManager.h"
#include "Paths.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include "AssetLoader.h"

SceneManager::SceneManager() {
}

bool SceneManager::prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) {
    try {
        if (data.header.assetType != AssetType::Scene) {
            SPDLOG_ERROR("Asset is not a scene");
            return false;
        }

        // Extract and store scene data
        auto [sceneInfo, sceneData] = AssetLib::ReadScene(data);
        m_loadedScenes[handle.filename] = sceneData;

        SPDLOG_INFO("Successfully prepared scene asset: {}", handle.filename);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to prepare scene asset {}: {}", handle.filename, e.what());
        return false;
    }
}

void SceneManager::unloadAsset(const std::string& filename) {
    auto it = m_loadedScenes.find(filename);
    if (it != m_loadedScenes.end()) {
        m_loadedScenes.erase(it);
        SPDLOG_INFO("Unloaded scene asset: {}", filename);
    }
}

bool SceneManager::isAssetReady(const std::string& filename) const {
    return m_loadedScenes.find(filename) != m_loadedScenes.end();
}

uint64_t SceneManager::getAssetSize(const std::string& filename) const {
    auto it = m_loadedScenes.find(filename);
    if (it != m_loadedScenes.end()) {
        return it->second.dump().size();
    }
    return 0;
}

std::vector<AssetDependency> SceneManager::getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const {
    // Scenes don't have external dependencies for now
    return {};
}

std::any SceneManager::getResourceInternal(const AssetHandle& handle) const {
    auto it = m_loadedScenes.find(handle.filename);
    if (it != m_loadedScenes.end()) {
        return it->second;
    }

    // If scene is not loaded, try to request it from AssetManager
    SPDLOG_INFO("Scene '{}' not found in cache, attempting to load it", handle.filename);
    if (requestAssetReady(handle)) {
        // After successful load, try to get the scene data again
        auto loadedIt = m_loadedScenes.find(handle.filename);
        if (loadedIt != m_loadedScenes.end()) {
            SPDLOG_INFO("Successfully loaded scene '{}' on demand", handle.filename);
            return loadedIt->second;
        }
    }

    SPDLOG_WARN("Failed to load scene '{}' on demand", handle.filename);
    return nlohmann::json{};
}

std::any SceneManager::getHandleInternal(const std::string& filename) const {
    // Scenes don't use handles like other assets
    return std::string{};
}

nlohmann::json SceneManager::getSceneData(const std::string& filename) const {
    auto it = m_loadedScenes.find(filename);
    if (it != m_loadedScenes.end()) {
        return it->second;
    }

    // If scene is not loaded, try to request it from AssetManager
    SPDLOG_INFO("Scene '{}' not found in cache, attempting to load it", filename);
    AssetHandle handle(AssetType::Scene, filename);
    if (requestAssetReady(handle)) {
        // After successful load, try to get the scene data again
        auto loadedIt = m_loadedScenes.find(filename);
        if (loadedIt != m_loadedScenes.end()) {
            SPDLOG_INFO("Successfully loaded scene '{}' on demand", filename);
            return loadedIt->second;
        }
    }

    SPDLOG_WARN("Failed to load scene '{}' on demand", filename);
    return nlohmann::json{};
}

bool SceneManager::hasSceneData(const std::string& filename) const {
    return m_loadedScenes.find(filename) != m_loadedScenes.end();
}

bool SceneManager::saveSceneToFile(const std::string& filename, const nlohmann::json& sceneData) {
    try {
        // Create AssetLib data structure
        AssetLib::SceneInfo sceneInfo;
        sceneInfo.entityCount = sceneData.contains("entities") ? sceneData["entities"].size() : 0;

        // Write scene asset
        AssetLib::AssetData assetData = AssetLib::WriteScene(filename, sceneInfo, sceneData);

        std::string filePath = getSceneFilePath(filename);
        AssetLib::WriteAsset(filePath, assetData);

        // Also update the in-memory cache with the new data
        m_loadedScenes[filename] = sceneData;

        SPDLOG_INFO("Successfully saved scene to file: {}", filename);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to save scene {}: {}", filename, e.what());
        return false;
    }
}

std::string SceneManager::getSceneFilePath(const std::string& filename) const {
    std::filesystem::path basePath = ASSETS_COMP;
    std::filesystem::path scenesDir = AssetLoader::GetAssetSubdirectory(AssetLib::AssetType::Scene);
    std::string extension = std::string(AssetLib::Utilities::GetAssetExtension(AssetLib::AssetType::Scene));

    std::filesystem::path fullPath = basePath / scenesDir / (filename + extension);

    // Upewnij się, że katalog istnieje
    std::filesystem::path parentDir = fullPath.parent_path();
    if (!std::filesystem::exists(parentDir)) {
        std::error_code ec;
        if (!std::filesystem::create_directories(parentDir, ec)) {
            SPDLOG_ERROR("Failed to create directory {}: {}", parentDir.string(), ec.message());
        }
    }

    return fullPath.string();
}