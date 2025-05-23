#pragma once
#include "AssetLoader.h"
#include "AssetHandle.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <unordered_set>
#include <type_traits>
#include <spdlog/spdlog.h>
#include "IAssetHandler.h"
#include <queue>
#include <any>
#include <set>
#include "VramManager.h"

class AssetManager {
public:
    explicit AssetManager(VramManager& vramManager);
    ~AssetManager();

    // Register a handler for a specific asset type
    void registerHandler(AssetType type, std::shared_ptr<IAssetHandler> handler);

    // Core asset management methods
    bool ensureLoaded(const AssetHandle handle);
    bool ensureReady(const AssetHandle handle);
    void unloadAsset(const AssetHandle& handle);
    void purgeUnusedAssets(float memoryThresholdPercentage, uint64_t ageThresholdFrames);
    void releaseAsset(const AssetHandle& handle);
    void advanceFrame();

    // Generic resource getter
    template<typename T>
    T getResource(const AssetHandle& handle) const {
        // Add detailed debugging for resource retrieval attempts
        SPDLOG_DEBUG("Attempting to get resource of type {} for asset '{}'",
            typeid(T).name(), handle.filename);

        auto handlerIt = m_handlers.find(handle.type);
        if (handlerIt == m_handlers.end()) {
            SPDLOG_WARN("No handler registered for asset type {}", static_cast<int>(handle.type));
            return T{};
        }

        auto handler = handlerIt->second;
        return handler->getResource<T>(handle);
    }

    template<typename HandleType>
    HandleType getHandle(const AssetHandle& handle) const {
        SPDLOG_DEBUG("Attempting to get handle of type {} for asset '{}'",
            typeid(HandleType).name(), handle.filename);

        auto handlerIt = m_handlers.find(handle.type);
        if (handlerIt == m_handlers.end()) {
            SPDLOG_WARN("No handler registered for asset type {}", static_cast<int>(handle.type));
            return HandleType{};
        }

        auto handler = handlerIt->second;
        return handler->getHandle<HandleType>(handle.filename);
    }

private:
    VramManager& m_vramManager;
    AssetLoader m_assetLoader;
    uint64_t m_currentFrame = 0;

    // Map of asset types to their handlers
    std::unordered_map<AssetType, std::shared_ptr<IAssetHandler>> m_handlers;

    // Cache for raw asset data
    std::unordered_map<std::string, AssetLib::AssetData> m_assetCache;

    // Assets marked for removal
    std::unordered_set<AssetHandle> m_markedForRemoval;

    // Asset usage tracking
    struct AssetUsageInfo {
        uint64_t lastUsedFrame;     // Frame when the asset was last actively used (ensureReady)
        uint64_t lastLoadedFrame;   // Frame when the asset was last loaded/referenced (ensureLoaded)
        bool isActivelyUsed;        // Whether it was used in the current frame
    };
    std::unordered_map<std::string, AssetUsageInfo> m_assetUsage;

    // Dependency tracking
    std::unordered_map<std::string, std::vector<AssetDependency>> m_assetDependencies;

    // Processing state tracking to prevent cyclic dependencies
    mutable std::set<std::string> m_processingAssets;

    // Helper method to create a unique cache key from an AssetHandle
    std::string createCacheKey(const AssetHandle& handle) const;

    // Updates the last used timestamp for an asset
    void updateUsageInfo(const AssetHandle& handle, bool activeUse);

    // Gets the handler for a specific asset type
    std::shared_ptr<IAssetHandler> getHandler(AssetType type) const;

    // Processes dependencies for the given asset
    bool processDependencies(const AssetHandle& handle, const std::vector<AssetDependency>& dependencies);

    // Check for cyclic dependencies
    bool detectCyclicDependency(const std::string& assetKey) const;

    // Reset active usage flags at the beginning of each frame
    void resetActiveUsageFlags();

    bool isActiveDependency(const AssetHandle& handle) const;
};