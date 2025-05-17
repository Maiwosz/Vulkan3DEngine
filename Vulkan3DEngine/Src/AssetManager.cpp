#include "AssetManager.h"
#include <json.hpp>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/fmt/ostr.h>

using json = nlohmann::json;

// Custom formatter for AssetHandle to use in logs
namespace fmt {
    template<>
    struct formatter<AssetHandle> {
        template<typename ParseContext>
        constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

        template<typename FormatContext>
        auto format(const AssetHandle& handle, FormatContext& ctx) const {
            const char* typeStr = "Unknown";
            switch (handle.type) {
            case AssetType::Mesh: typeStr = "Mesh"; break;
            case AssetType::Texture: typeStr = "Texture"; break;
            case AssetType::Shader: typeStr = "Shader"; break;
            case AssetType::Material: typeStr = "Material"; break;
            default: break;
            }
            return fmt::format_to(ctx.out(), "{}:{}", typeStr, handle.filename);
        }
    };
}

std::string AssetManager::createCacheKey(const AssetHandle& handle) const {
    return std::to_string(static_cast<int>(handle.type)) + ":" + handle.filename;
}

AssetManager::AssetManager()
    : m_assetLoader() {
    SPDLOG_INFO("AssetManager initialized with");
}

AssetManager::~AssetManager() {
    // Clear all registered handlers
    m_handlers.clear();

    // Clear cached asset data
    m_assetCache.clear();

    SPDLOG_INFO("AssetManager destroyed");
}

void AssetManager::registerHandler(AssetType type, std::shared_ptr<IAssetHandler> handler) {
    SPDLOG_INFO("Registering asset handler for type {}", static_cast<int>(type));
    m_handlers[type] = std::move(handler);
}

std::shared_ptr<IAssetHandler> AssetManager::getHandler(AssetType type) const {
    auto it = m_handlers.find(type);
    if (it == m_handlers.end()) {
        return nullptr;
    }
    return it->second;
}

bool AssetManager::detectCyclicDependency(const std::string& assetKey) const {
    return m_processingAssets.find(assetKey) != m_processingAssets.end();
}

bool AssetManager::processDependencies(const AssetHandle& handle, const std::vector<AssetDependency>& dependencies) {
    std::string cacheKey = createCacheKey(handle);

    // Store dependencies for future reference
    m_assetDependencies[cacheKey] = dependencies;

    // Process each dependency based on its type
    for (const auto& dependency : dependencies) {
        std::string depCacheKey = createCacheKey(dependency.handle);

        // Check for cyclic dependencies
        if (detectCyclicDependency(depCacheKey)) {
            SPDLOG_ERROR("Cyclic dependency detected for asset {}", handle);
            return false;
        }

        // Add to processing set to detect cycles
        m_processingAssets.insert(cacheKey);

        try {
            switch (dependency.type) {
            case DependencyType::LoadTime:
                // Ensure the dependency is loaded
                if (!ensureLoaded(dependency.handle)) {
                    SPDLOG_ERROR("Failed to load dependency {} for asset {}", dependency.handle, handle);
                    m_processingAssets.erase(cacheKey);
                    return false;
                }
                break;

            case DependencyType::PrepareTime:
                // Ensure the dependency is loaded but not necessarily prepared
                if (!ensureLoaded(dependency.handle)) {
                    SPDLOG_ERROR("Failed to load preparation dependency {} for asset {}", dependency.handle, handle);
                    m_processingAssets.erase(cacheKey);
                    return false;
                }
                break;

            case DependencyType::UsageTime:
                // Ensure the dependency is fully prepared
                if (!ensureReady(dependency.handle)) {
                    SPDLOG_ERROR("Failed to prepare usage dependency {} for asset {}", dependency.handle, handle);
                    m_processingAssets.erase(cacheKey);
                    return false;
                }
                break;
            }
        }
        catch (...) {
            m_processingAssets.erase(cacheKey);
            throw; // Re-throw the exception
        }

        // Remove from processing set after handling
        m_processingAssets.erase(cacheKey);
    }

    return true;
}

bool AssetManager::ensureLoaded(const AssetHandle handle) {
    if (m_markedForRemoval.count(handle)) {
        m_markedForRemoval.erase(handle);
    }

    // Create a key that includes both filename and type
    std::string cacheKey = createCacheKey(handle);

    // Check if already in cache
    if (m_assetCache.count(cacheKey) == 0) {
        SPDLOG_INFO("Loading asset: {}", handle);
        try {
            AssetLib::AssetData data = m_assetLoader.load(handle);
            if (data.header.assetType != handle.type) {
                SPDLOG_ERROR("Type mismatch for {}: Expected {}, got {}",
                    handle.filename, static_cast<int>(handle.type), static_cast<int>(data.header.assetType));
                return false; // Type mismatch
            }

            // Get handler for this asset type
            auto handler = getHandler(handle.type);
            if (!handler) {
                SPDLOG_ERROR("No handler registered for asset type {}", static_cast<int>(handle.type));
                return false;
            }

            // Extract and process dependencies
            auto dependencies = handler->getDependencies(handle, data);
            if (!processDependencies(handle, dependencies)) {
                SPDLOG_ERROR("Failed to process dependencies for asset {}", handle);
                return false;
            }

            // Store asset data in cache
            m_assetCache.emplace(cacheKey, std::move(data));
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("Exception while loading asset {}: {}", handle, e.what());
            return false;
        }
        catch (...) {
            SPDLOG_ERROR("Unknown exception while loading asset {}", handle);
            return false;
        }
    }

    updateLastUsed(handle);
    return true;
}

bool AssetManager::ensureReady(const AssetHandle handle) {
    if (m_markedForRemoval.count(handle)) {
        m_markedForRemoval.erase(handle);
    }

    auto handler = getHandler(handle.type);
    if (!handler) {
        SPDLOG_ERROR("No handler registered for asset type {}", static_cast<int>(handle.type));
        return false;
    }

    // Check if the asset is already ready
    if (handler->isAssetReady(handle.filename)) {
        updateLastUsed(handle);
        return true;
    }

    // Ensure asset is loaded first
    if (!ensureLoaded(handle)) {
        SPDLOG_ERROR("Failed to load asset {}", handle);
        return false;
    }

    // Get the asset data and prepare it for use
    std::string cacheKey = createCacheKey(handle);
    auto assetDataIt = m_assetCache.find(cacheKey);
    if (assetDataIt == m_assetCache.end()) {
        SPDLOG_ERROR("Asset data not found in cache: {}", handle);
        return false;
    }

    // Check UsageTime dependencies to ensure they are ready
    auto depIt = m_assetDependencies.find(cacheKey);
    if (depIt != m_assetDependencies.end()) {
        for (const auto& dep : depIt->second) {
            if (dep.type == DependencyType::UsageTime) {
                if (!ensureReady(dep.handle)) {
                    SPDLOG_ERROR("Failed to prepare usage dependency {} for asset {}", dep.handle, handle);
                    return false;
                }
            }
        }
    }

    // Delegate preparation to the handler
    try {
        if (!handler->prepareAsset(handle, assetDataIt->second, *this)) {
            SPDLOG_ERROR("Failed to prepare asset {} for use", handle);
            return false;
        }
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during asset preparation: {} for asset {}", e.what(), handle);
        return false;
    }
    catch (...) {
        SPDLOG_ERROR("Unknown exception during asset preparation for {}", handle);
        return false;
    }

    updateLastUsed(handle);
    SPDLOG_INFO("Asset {} is now ready", handle);
    return true;
}

void AssetManager::unloadAsset(const AssetHandle& handle) {
    SPDLOG_INFO("Unloading asset: {}", handle);

    auto handler = getHandler(handle.type);
    if (!handler) {
        SPDLOG_ERROR("No handler registered for asset type {}", static_cast<int>(handle.type));
        return;
    }

    // Unload the asset using its handler
    try {
        handler->unloadAsset(handle.filename);

        // Remove from asset cache
        std::string cacheKey = createCacheKey(handle);
        m_assetCache.erase(cacheKey);

        // Remove dependencies
        m_assetDependencies.erase(cacheKey);

        // Remove from usage tracking
        m_assetLastUsed.erase(cacheKey);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during asset unloading: {} for asset {}", e.what(), handle);
    }
}

void AssetManager::purgeUnusedAssets(float memoryThresholdPercentage, uint64_t ageThresholdFrames) {
    SPDLOG_INFO("Purging unused assets - memory threshold: {}%, age threshold: {} frames",
        memoryThresholdPercentage * 100.0f, ageThresholdFrames);

    // Process assets marked for removal first
    auto markIt = m_markedForRemoval.begin();
    while (markIt != m_markedForRemoval.end()) {
        const AssetHandle& handle = *markIt;
        unloadAsset(handle);
        markIt = m_markedForRemoval.erase(markIt);
    }

    // Age-based unloading
    if (ageThresholdFrames > 0) {
        std::vector<AssetHandle> toUnload;

        // Find assets older than the threshold
        for (const auto& [cacheKey, lastUsed] : m_assetLastUsed) {
            if (m_currentFrame - lastUsed > ageThresholdFrames) {
                // Extract type and filename from cache key
                size_t separatorPos = cacheKey.find(':');
                if (separatorPos != std::string::npos) {
                    AssetType type = static_cast<AssetType>(std::stoi(cacheKey.substr(0, separatorPos)));
                    std::string filename = cacheKey.substr(separatorPos + 1);
                    toUnload.emplace_back(type, filename);
                }
            }
        }

        // Unload aged assets
        if (!toUnload.empty()) {
            SPDLOG_INFO("Age-based unload: found {} assets to unload", toUnload.size());
            for (const auto& handle : toUnload) {
                unloadAsset(handle);
            }
        }
    }

    // Memory-based unloading
    if (memoryThresholdPercentage > 0.0f) {
        // Collect memory usage statistics
        uint64_t totalVramUsed = 0;
        uint64_t vramBudget = 0; // Get from VRAM handlers

        // Collect all assets that can be unloaded
        struct AssetInfo {
            AssetHandle handle;
            uint64_t size;
            uint64_t lastUsed;
            bool isVram;
        };
        std::vector<AssetInfo> assets;

        // Gather statistics and collect assets
        for (const auto& [type, handler] : m_handlers) {
            bool isVram = handler->isInVram();

            // For each asset type, check ready assets
            for (const auto& [cacheKey, lastUsed] : m_assetLastUsed) {
                // Extract type and filename from cache key
                size_t separatorPos = cacheKey.find(':');
                if (separatorPos == std::string::npos) continue;

                AssetType assetType = static_cast<AssetType>(std::stoi(cacheKey.substr(0, separatorPos)));
                if (assetType != type) continue;

                std::string filename = cacheKey.substr(separatorPos + 1);
                uint64_t size = handler->getAssetSize(filename);

                if (isVram) {
                    totalVramUsed += size;
                }

                assets.push_back({
                    AssetHandle(type, filename),
                    size,
                    lastUsed,
                    isVram
                    });
            }
        }

        // Sort assets by last used time (oldest first)
        std::sort(assets.begin(), assets.end(), [](const auto& a, const auto& b) {
            return a.lastUsed < b.lastUsed;
            });

        // If VRAM usage exceeds threshold, unload assets
        if (vramBudget > 0 && totalVramUsed > vramBudget * memoryThresholdPercentage) {
            uint64_t targetUsage = vramBudget * memoryThresholdPercentage;
            uint64_t freedMemory = 0;
            size_t unloadedCount = 0;

            for (const auto& asset : assets) {
                if (!asset.isVram) continue;
                if (totalVramUsed <= targetUsage) break;

                unloadAsset(asset.handle);
                totalVramUsed -= asset.size;
                freedMemory += asset.size;
                unloadedCount++;
            }

            if (unloadedCount > 0) {
                SPDLOG_INFO("VRAM cleanup: unloaded {} assets, freed {} bytes",
                    unloadedCount, freedMemory);
            }
        }
    }
}

void AssetManager::releaseAsset(const AssetHandle& handle) {
    // Mark for removal if it's ready
    auto handler = getHandler(handle.type);
    if (handler && handler->isAssetReady(handle.filename)) {
        SPDLOG_INFO("Asset {} marked for removal", handle);
        m_markedForRemoval.insert(handle);
    }
    else {
        // If not ready, we can unload immediately
        std::string cacheKey = createCacheKey(handle);
        m_assetCache.erase(cacheKey);
        m_assetDependencies.erase(cacheKey);
        m_assetLastUsed.erase(cacheKey);
    }
}

void AssetManager::advanceFrame() {
    ++m_currentFrame;

    // Periodic cleanup can be performed here
    // For example, every 100 frames
    if (m_currentFrame % 100 == 0) {
        purgeUnusedAssets(0.8f, 300);
    }
}

void AssetManager::updateLastUsed(const AssetHandle& handle) {
    std::string cacheKey = createCacheKey(handle);
    m_assetLastUsed[cacheKey] = m_currentFrame;
}
