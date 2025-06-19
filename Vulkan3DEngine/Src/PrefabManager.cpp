#include "PrefabManager.h"
#include "Paths.h"
#include <spdlog/spdlog.h>
#include <filesystem>

PrefabManager::PrefabManager() {
}

bool PrefabManager::prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) {
    try {
        if (data.header.assetType != AssetType::Prefab) {
            SPDLOG_ERROR("Asset is not a prefab");
            return false;
        }

        // Extract and deserialize prefab data from asset
        auto [prefabInfo, prefabData] = AssetLib::ReadPrefab(data);

        // Deserialize to internal prefab structure
        Prefab prefab = deserializePrefab(prefabData);

        // Create prefab handle and store
        PrefabHandle prefabHandle = createPrefab(prefab, handle.filename);

        SPDLOG_INFO("Successfully prepared prefab asset: {}", handle.filename);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to prepare prefab asset {}: {}", handle.filename, e.what());
        return false;
    }
}

void PrefabManager::unloadAsset(const std::string& filename) {
    auto it = m_prefabHandles.find(filename);
    if (it != m_prefabHandles.end()) {
        PrefabHandle handle = it->second;
        m_prefabs.erase(handle.id);
        m_prefabHandles.erase(it);
        SPDLOG_INFO("Unloaded prefab asset: {}", filename);
    }
}

bool PrefabManager::isAssetReady(const std::string& filename) const {
    auto it = m_prefabHandles.find(filename);
    if (it != m_prefabHandles.end()) {
        return m_prefabs.find(it->second.id) != m_prefabs.end();
    }
    return false;
}

uint64_t PrefabManager::getAssetSize(const std::string& filename) const {
    auto it = m_prefabHandles.find(filename);
    if (it != m_prefabHandles.end()) {
        auto prefabIt = m_prefabs.find(it->second.id);
        if (prefabIt != m_prefabs.end()) {
            // Approximate size calculation
            size_t size = sizeof(Prefab);
            for (const auto& [entity, entityData] : prefabIt->second.entities) {
                size += entityData.name.size();
                size += entityData.children.size() * sizeof(Entity);
                for (const auto& [componentType, componentData] : entityData.components) {
                    size += componentType.size() + componentData.dump().size();
                }
            }
            return size;
        }
    }
    return 0;
}

std::vector<AssetDependency> PrefabManager::getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const {
    return {}; // No external dependencies for now
}

std::any PrefabManager::getResourceInternal(const AssetHandle& handle) const {
    auto it = m_prefabHandles.find(handle.filename);
    if (it != m_prefabHandles.end()) {
        const Prefab* prefab = getPrefab(it->second);
        if (prefab) {
            return *prefab;
        }
    }
    return {};
}

std::any PrefabManager::getHandleInternal(const std::string& filename) const {
    auto it = m_prefabHandles.find(filename);
    if (it != m_prefabHandles.end()) {
        return it->second;
    }
    return PrefabHandle{};
}

const Prefab* PrefabManager::getPrefab(PrefabHandle handle) const {
    auto it = m_prefabs.find(handle.id);
    return it != m_prefabs.end() ? &it->second : nullptr;
}

PrefabHandle PrefabManager::createPrefab(const Prefab& prefabData, const std::string& filename) {
    PrefabHandle handle(m_nextHandleId++);
    m_prefabs[handle.id] = prefabData;
    m_prefabHandles[filename] = handle;
    return handle;
}

bool PrefabManager::savePrefabToFile(PrefabHandle handle) {
    auto prefabIt = m_prefabs.find(handle.id);
    if (prefabIt == m_prefabs.end()) {
        SPDLOG_ERROR("Prefab not found for handle");
        return false;
    }

    try {
        // Find filename for this handle
        std::string filename;
        for (const auto& [fname, phandle] : m_prefabHandles) {
            if (phandle.id == handle.id) {
                filename = fname;
                break;
            }
        }

        if (filename.empty()) {
            SPDLOG_ERROR("No filename found for prefab handle");
            return false;
        }

        std::string filePath = getPrefabFilePath(filename);

        // Create directory if it doesn't exist
        std::filesystem::path dir = std::filesystem::path(filePath).parent_path();
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        // Serialize prefab to JSON
        nlohmann::json jsonData = serializePrefab(prefabIt->second);

        // Create AssetData for writing
        AssetLib::PrefabInfo info;
        info.entityCount = prefabIt->second.entityCount;

        AssetLib::AssetData assetData = AssetLib::WritePrefab(filename, info, jsonData);
        AssetLib::WriteAsset(filePath, assetData);

        SPDLOG_INFO("Saved prefab to file: {}", filePath);
        return true;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to save prefab to file: {}", e.what());
        return false;
    }
}

nlohmann::json PrefabManager::serializePrefab(const Prefab& prefab) const {
    nlohmann::json result;

    result["rootEntity"] = prefab.rootEntity.id;
    result["entityCount"] = prefab.entityCount;

    nlohmann::json entitiesJson = nlohmann::json::object();
    for (const auto& [entity, entityData] : prefab.entities) {
        nlohmann::json entityJson;
        entityJson["name"] = entityData.name;

        // Serialize parent information
        entityJson["parent"] = entityData.parent.id;

        // Serialize children
        nlohmann::json childrenJson = nlohmann::json::array();
        for (Entity child : entityData.children) {
            childrenJson.push_back(child.id);
        }
        entityJson["children"] = childrenJson;

        // Serialize components
        entityJson["components"] = entityData.components;

        entitiesJson[std::to_string(entity.id)] = entityJson;
    }

    result["entities"] = entitiesJson;
    return result;
}

Prefab PrefabManager::deserializePrefab(const nlohmann::json& data) const {
    Prefab prefab;

    prefab.rootEntity = Entity(data["rootEntity"]);
    prefab.entityCount = data["entityCount"];

    const auto& entitiesJson = data["entities"];
    for (const auto& [entityIdStr, entityJson] : entitiesJson.items()) {
        Entity entity(std::stoul(entityIdStr));

        PrefabEntity entityData;
        entityData.name = entityJson["name"];

        // Deserialize parent information
        if (entityJson.contains("parent")) {
            entityData.parent = Entity(entityJson["parent"]);
        }

        // Deserialize children
        if (entityJson.contains("children")) {
            for (const auto& childIdJson : entityJson["children"]) {
                entityData.children.push_back(Entity(childIdJson));
            }
        }

        // Deserialize components
        if (entityJson.contains("components")) {
            entityData.components = entityJson["components"];
        }

        prefab.entities[entity] = std::move(entityData);
    }

    return prefab;
}

std::string PrefabManager::getPrefabFilePath(const std::string& filename) const {
    std::filesystem::path basePath = ASSETS_COMP;
    std::filesystem::path scenesDir = AssetLoader::GetAssetSubdirectory(AssetLib::AssetType::Prefab);
    std::string extension = std::string(AssetLib::Utilities::GetAssetExtension(AssetLib::AssetType::Prefab));

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