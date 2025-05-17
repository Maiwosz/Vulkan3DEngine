#pragma once
#include "AssetHandle.h"
#include "Handle.h"
#include <memory>
#include <string>
#include <any>
#include <vector>
#include <unordered_map>

// Forward declarations
class AssetManager;

// Dependency type enumeration
enum class DependencyType {
    LoadTime,      // Asset must be loaded before dependent asset is loaded
    PrepareTime,   // Asset must be loaded before dependent asset is prepared
    UsageTime      // Asset must be prepared before dependent asset is prepared
};

// Dependency structure
struct AssetDependency {
    AssetHandle handle;
    DependencyType type;
    std::any configuration;  // Optional configuration data for the dependency

    // Constructor for easier creation
    AssetDependency(const AssetHandle& h, DependencyType t, std::any config = std::any())
        : handle(h), type(t), configuration(std::move(config)) {
    }

    // Helper constructor for texture dependencies with color space
    static AssetDependency createTextureDependency(
        const AssetHandle& textureHandle,
        DependencyType type,
        AssetLib::ColorSpace colorSpace
    ) {
        std::unordered_map<std::string, std::any> config;
        config["colorSpace"] = colorSpace;
        return AssetDependency(textureHandle, type, config);
    }
};

// Interface for all asset handlers
class IAssetHandler {
public:
    virtual ~IAssetHandler() = default;

    // Core asset lifecycle methods
    virtual bool prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) = 0;
    virtual void unloadAsset(const std::string& filename) = 0;
    virtual bool isAssetReady(const std::string& filename) const = 0;

    // Memory management
    virtual uint64_t getAssetSize(const std::string& filename) const = 0;
    virtual bool isInVram() const = 0;  // Indicates if this handler manages VRAM resources

    // Dependency management
    virtual std::vector<AssetDependency> getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const = 0;

    // Resource retrieval - Use type erasure instead of templates
    virtual std::any getResourceInternal(const AssetHandle& handle) const = 0;

    // Resource accessor (public interface)
    template<typename T>
    T getResource(const AssetHandle& handle) const {
        return std::any_cast<T>(getResourceInternal(handle));
    }

    // Handle retrieval - generic interface for getting handles by filename
    virtual std::any getHandleInternal(const std::string& filename) const = 0;

    // Handle accessor (public interface)
    template<typename HandleType>
    HandleType getHandle(const std::string& filename) const {
        return std::any_cast<HandleType>(getHandleInternal(filename));
    }
};