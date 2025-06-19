#include "AssetManager.h"
#include <json.hpp>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/fmt/ostr.h>
#include <algorithm>

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
            case AssetType::Prefab: typeStr = "Prefab"; break;
            case AssetType::Scene: typeStr = "Scene"; break;
            default: break;
            }
            return fmt::format_to(ctx.out(), "{}:{}", typeStr, handle.filename);
        }
    };
}

std::string AssetManager::createCacheKey(const AssetHandle& handle) const {
    return std::to_string(static_cast<int>(handle.type)) + ":" + handle.filename;
}

AssetManager::AssetManager(VramManager& vramManager)
    : m_assetLoader(), m_vramManager(vramManager) {
    SPDLOG_INFO("AssetManager initialized");
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

void AssetManager::updateUsageInfo(const AssetHandle& handle, bool activeUse) {
    std::string cacheKey = createCacheKey(handle);
    auto& usage = m_assetUsage[cacheKey];

    // Always update the loaded timestamp
    usage.lastLoadedFrame = m_currentFrame;

    // Only update active usage for ensureReady calls
    if (activeUse) {
        usage.lastUsedFrame = m_currentFrame;
        usage.isActivelyUsed = true;
        SPDLOG_DEBUG("Asset {} actively used in frame {}", handle, m_currentFrame);
    }
}

void AssetManager::resetActiveUsageFlags() {
    for (auto& [key, info] : m_assetUsage) {
        info.isActivelyUsed = false;
    }
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

    // Update usage info - not actively using the asset, just loading it
    updateUsageInfo(handle, false);
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
        // Mark as actively used because ensureReady was called
        updateUsageInfo(handle, true);

        // IMPORTANT FIX: Even if the asset is ready, we need to mark its dependencies as used
        std::string cacheKey = createCacheKey(handle);
        auto depIt = m_assetDependencies.find(cacheKey);
        if (depIt != m_assetDependencies.end()) {
            for (const auto& dep : depIt->second) {
                // Mark all dependencies as used, not just UsageTime dependencies
                updateUsageInfo(dep.handle, true);  // Mark dependency as actively used
            }
        }

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

    // Process ALL dependencies, not just UsageTime
    auto depIt = m_assetDependencies.find(cacheKey);
    if (depIt != m_assetDependencies.end()) {
        for (const auto& dep : depIt->second) {
            // For UsageTime dependencies, ensure they are ready
            if (dep.type == DependencyType::UsageTime) {
                if (!ensureReady(dep.handle)) {
                    SPDLOG_ERROR("Failed to prepare usage dependency {} for asset {}", dep.handle, handle);
                    return false;
                }
            }
            // For other dependencies, at least mark them as actively used
            else {
                updateUsageInfo(dep.handle, true);  // Mark dependency as actively used
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

    // Mark as actively used
    updateUsageInfo(handle, true);
    SPDLOG_INFO("Asset {} is now ready and actively used", handle);
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
        m_assetUsage.erase(cacheKey);
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

        // IMPORTANT FIX: Check if this asset is a dependency of any active asset
        bool isDependencyOfActiveAsset = false;
        std::string handleCacheKey = createCacheKey(handle);

        // Check if any asset that depends on this one is active
        for (const auto& [depCacheKey, deps] : m_assetDependencies) {
            // Skip checking if we already know it's a dependency
            if (isDependencyOfActiveAsset) break;

            // Get usage info for the parent asset
            auto usageIt = m_assetUsage.find(depCacheKey);
            if (usageIt != m_assetUsage.end() && usageIt->second.isActivelyUsed) {
                // If parent is active, check if this asset is one of its dependencies
                for (const auto& dep : deps) {
                    std::string depHandleCacheKey = createCacheKey(dep.handle);
                    if (depHandleCacheKey == handleCacheKey) {
                        isDependencyOfActiveAsset = true;
                        SPDLOG_DEBUG("Asset {} is a dependency of active asset, skipping removal", handle);
                        break;
                    }
                }
            }
        }

        if (!isDependencyOfActiveAsset) {
            unloadAsset(handle);
        }
        else {
            // Don't unload, but remove from marked for removal list
            SPDLOG_INFO("Asset {} is a dependency of an active asset, canceling removal", handle);
        }

        markIt = m_markedForRemoval.erase(markIt);
    }

    // Build a set of assets that are dependencies of active assets
    std::unordered_set<std::string> activeDependencies;
    for (const auto& [cacheKey, usageInfo] : m_assetUsage) {
        if (usageInfo.isActivelyUsed) {
            // Add all dependencies of this active asset
            auto depIt = m_assetDependencies.find(cacheKey);
            if (depIt != m_assetDependencies.end()) {
                for (const auto& dep : depIt->second) {
                    std::string depCacheKey = createCacheKey(dep.handle);
                    activeDependencies.insert(depCacheKey);
                }
            }
        }
    }

    // Age-based unloading
    if (ageThresholdFrames > 0) {
        std::vector<AssetHandle> toUnload;

        // Find assets older than the threshold by checking lastUsedFrame (not lastLoadedFrame)
        for (const auto& [cacheKey, usageInfo] : m_assetUsage) {
            uint64_t age = m_currentFrame - usageInfo.lastUsedFrame;

            if (age > ageThresholdFrames) {
                // Skip if this is a dependency of an active asset
                if (activeDependencies.count(cacheKey) > 0) {
                    SPDLOG_DEBUG("Skipping aged asset {} as it's a dependency of an active asset", cacheKey);
                    continue;
                }

                // Extract type and filename from cache key
                size_t separatorPos = cacheKey.find(':');
                if (separatorPos != std::string::npos) {
                    AssetType type = static_cast<AssetType>(std::stoi(cacheKey.substr(0, separatorPos)));
                    std::string filename = cacheKey.substr(separatorPos + 1);
                    SPDLOG_DEBUG("Asset {}:{} is aged (last used {} frames ago)",
                        static_cast<int>(type), filename, age);
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

    // Memory-based unloading (rest of the method remains unchanged)
    if (memoryThresholdPercentage > 0.0f) {
        // Get VRAM information from VramManager
        uint64_t totalVramUsed = m_vramManager.getVramUsed();
        uint64_t vramBudget = m_vramManager.getVramBudget();
        float currentUsagePercentage = m_vramManager.getVramUsagePercentage();

        SPDLOG_INFO("VRAM status: used={:.2f}MB, budget={:.2f}MB, usage={:.1f}%",
            totalVramUsed / (1024.0f * 1024.0f), vramBudget / (1024.0f * 1024.0f), currentUsagePercentage);

        // Only unload if we're exceeding the threshold percentage
        if (vramBudget > 0 && currentUsagePercentage > memoryThresholdPercentage * 100.0f) {
            // Collect all assets that can be unloaded
            struct AssetInfo {
                AssetHandle handle;
                uint64_t size;
                uint64_t lastUsedFrame;
                bool isVram;
            };
            std::vector<AssetInfo> assets;

            // Gather statistics and collect assets
            for (const auto& [type, handler] : m_handlers) {
                bool isVram = handler->isInVram();

                for (const auto& [cacheKey, usageInfo] : m_assetUsage) {
                    // Skip if this is a dependency of an active asset
                    if (activeDependencies.count(cacheKey) > 0) {
                        continue;
                    }

                    // Extract type and filename from cache key
                    size_t separatorPos = cacheKey.find(':');
                    if (separatorPos == std::string::npos) continue;

                    AssetType assetType = static_cast<AssetType>(std::stoi(cacheKey.substr(0, separatorPos)));
                    if (assetType != type) continue;

                    std::string filename = cacheKey.substr(separatorPos + 1);
                    uint64_t size = handler->getAssetSize(filename);

                    assets.push_back({
                        AssetHandle(type, filename),
                        size,
                        usageInfo.lastUsedFrame,  // Use lastUsedFrame for prioritization
                        isVram
                        });
                }
            }

            // Sort assets by last used frame (oldest first)
            std::sort(assets.begin(), assets.end(), [](const auto& a, const auto& b) {
                return a.lastUsedFrame < b.lastUsedFrame;
                });

            // Calculate target usage
            uint64_t targetUsage = static_cast<uint64_t>(vramBudget * (memoryThresholdPercentage));
            uint64_t needToFree = (totalVramUsed > targetUsage) ? (totalVramUsed - targetUsage) : 0;

            if (needToFree > 0) {
                uint64_t freedMemory = 0;
                size_t unloadedCount = 0;

                SPDLOG_INFO("Memory threshold exceeded. Need to free {:.2f} MB to reach target of {:.1f}%",
                    needToFree / (1024.0f * 1024.0f),
                    memoryThresholdPercentage * 100.0f);

                for (const auto& asset : assets) {
                    if (!asset.isVram) continue;
                    if (freedMemory >= needToFree) break;

                    // Skip currently active assets
                    std::string assetKey = createCacheKey(asset.handle);
                    auto usageIt = m_assetUsage.find(assetKey);
                    if (usageIt != m_assetUsage.end() && usageIt->second.isActivelyUsed) {
                        SPDLOG_DEBUG("Skipping active asset {} during VRAM cleanup", asset.handle);
                        continue;
                    }

                    unloadAsset(asset.handle);
                    freedMemory += asset.size;
                    unloadedCount++;
                }

                if (unloadedCount > 0) {
                    SPDLOG_INFO("VRAM cleanup: unloaded {} assets, freed {:.2f} MB",
                        unloadedCount, freedMemory / (1024.0f * 1024.0f));
                }
            }
            else {
                SPDLOG_INFO("Memory threshold not exceeded, no assets to unload");
            }
        }
        else {
            SPDLOG_INFO("Current VRAM usage {:.1f}% is below threshold {:.1f}%, skipping memory-based unloading",
                currentUsagePercentage, memoryThresholdPercentage * 100.0f);
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
        m_assetUsage.erase(cacheKey);
    }
}

void AssetManager::advanceFrame() {
    ++m_currentFrame;

    resetActiveUsageFlags();
    purgeUnusedAssets(0.8f, 10000);
}

bool AssetManager::isActiveDependency(const AssetHandle& handle) const {
    std::string handleCacheKey = createCacheKey(handle);

    // Check all active assets
    for (const auto& [cacheKey, usageInfo] : m_assetUsage) {
        if (usageInfo.isActivelyUsed) {
            // Check if handle is a dependency of this active asset
            auto depIt = m_assetDependencies.find(cacheKey);
            if (depIt != m_assetDependencies.end()) {
                for (const auto& dep : depIt->second) {
                    if (createCacheKey(dep.handle) == handleCacheKey) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}