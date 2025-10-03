#pragma once
#include "AssetHandle.h"
#include "Handle.h"
#include <memory>
#include <string>
#include <any>
#include <vector>
#include <unordered_map>
#include <functional>

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

// Callback types for AssetManager communication
using AssetLoadCallback = std::function<bool(const AssetHandle&)>;
using AssetReadyCallback = std::function<bool(const AssetHandle&)>;

// Forward declaration for SmartAssetHandle template
template<typename HandleType, typename ResourceType>
class ISmartAssetHandler;

// Smart handle for assets - no reference counting, just convenient access
template<typename HandleType, typename ResourceType>
class SmartAssetHandle {
public:
    // Default constructor - invalid handle
    SmartAssetHandle() : m_handle(), m_handler(nullptr) {}

    // Copy constructor
    SmartAssetHandle(const SmartAssetHandle& other)
        : m_handle(other.m_handle), m_handler(other.m_handler) {
    }

    // Move constructor
    SmartAssetHandle(SmartAssetHandle&& other) noexcept
        : m_handle(other.m_handle), m_handler(other.m_handler) {
        other.m_handle = HandleType{};
        other.m_handler = nullptr;
    }

    // Copy assignment
    SmartAssetHandle& operator=(const SmartAssetHandle& other) {
        if (this != &other) {
            m_handle = other.m_handle;
            m_handler = other.m_handler;
        }
        return *this;
    }

    // Move assignment
    SmartAssetHandle& operator=(SmartAssetHandle&& other) noexcept {
        if (this != &other) {
            m_handle = other.m_handle;
            m_handler = other.m_handler;
            other.m_handle = HandleType{};
            other.m_handler = nullptr;
        }
        return *this;
    }

    // Resource access
    ResourceType* get() const {
        return m_handler ? m_handler->getResource(m_handle) : nullptr;
    }

    ResourceType* operator->() const {
        return get();
    }

    ResourceType& operator*() const {
        ResourceType* ptr = get();
        if (!ptr) {
            throw std::runtime_error("Dereferencing invalid SmartAssetHandle");
        }
        return *ptr;
    }

    // Handle access
    HandleType handle() const { return m_handle; }

    // Validity check
    bool isValid() const {
        return m_handler && m_handle.isValid() && m_handler->isAssetReady(m_handle);
    }

    explicit operator bool() const { return isValid(); }

    // Comparison operators
    bool operator==(const SmartAssetHandle& other) const {
        return m_handle == other.m_handle && m_handler == other.m_handler;
    }

    bool operator!=(const SmartAssetHandle& other) const {
        return !(*this == other);
    }

    // Reset to invalid state
    void reset() {
        m_handle = HandleType{};
        m_handler = nullptr;
    }

    // Get the handler (for advanced use cases)
    ISmartAssetHandler<HandleType, ResourceType>* getHandler() const {
        return m_handler;
    }

private:
    // Constructor for handler use only
    SmartAssetHandle(HandleType handle, ISmartAssetHandler<HandleType, ResourceType>* handler)
        : m_handle(handle), m_handler(handler) {
    }

    friend class ISmartAssetHandler<HandleType, ResourceType>;

    HandleType m_handle;
    ISmartAssetHandler<HandleType, ResourceType>* m_handler;
};

// Base interface for all asset handlers
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

    // AssetManager callback registration
    void setAssetManagerCallbacks(AssetLoadCallback loadCallback, AssetReadyCallback readyCallback) {
        m_loadAssetCallback = std::move(loadCallback);
        m_ensureReadyCallback = std::move(readyCallback);
    }

protected:
    // Helper methods for handlers to request assets from AssetManager
    bool requestAssetLoad(const AssetHandle& handle) const {
        if (m_loadAssetCallback) {
            return m_loadAssetCallback(handle);
        }
        return false;
    }

    bool requestAssetReady(const AssetHandle& handle) const {
        if (m_ensureReadyCallback) {
            return m_ensureReadyCallback(handle);
        }
        return false;
    }

    // Convenience methods for specific asset types
    bool requestAssetLoad(AssetType type, const std::string& filename) const {
        return requestAssetLoad(AssetHandle(type, filename));
    }

    bool requestAssetReady(AssetType type, const std::string& filename) const {
        return requestAssetReady(AssetHandle(type, filename));
    }

private:
    AssetLoadCallback m_loadAssetCallback;
    AssetReadyCallback m_ensureReadyCallback;
};

// Extended interface for handlers that support smart handles
template<typename HandleType, typename ResourceType>
class ISmartAssetHandler : public IAssetHandler {
public:
    virtual ~ISmartAssetHandler() = default;

    // Typed resource access methods
    virtual ResourceType* getResource(HandleType handle) const = 0;
    virtual bool isAssetReady(HandleType handle) const = 0;

    // Factory method for creating smart handles from existing handles
    SmartAssetHandle<HandleType, ResourceType> createSmartHandle(HandleType handle) const {
        if (isAssetReady(handle)) {
            return SmartAssetHandle<HandleType, ResourceType>(handle, const_cast<ISmartAssetHandler*>(this));
        }
        return SmartAssetHandle<HandleType, ResourceType>(); // Invalid handle
    }

    // Factory method for creating smart handles from filename
    SmartAssetHandle<HandleType, ResourceType> createSmartHandle(const std::string& filename) const {
        HandleType handle = getHandle<HandleType>(filename);
        return createSmartHandle(handle);
    }

    // Convenience method for getting smart handle by filename with automatic loading
    SmartAssetHandle<HandleType, ResourceType> getSmartAsset(const std::string& filename) const {
        // Try to get existing handle first
        HandleType handle = getHandle<HandleType>(filename);
        if (handle.isValid() && isAssetReady(handle)) {
            return createSmartHandle(handle);
        }

        // If not ready, we can't create a valid smart handle
        // The caller should ensure the asset is loaded first
        return SmartAssetHandle<HandleType, ResourceType>();
    }
};

// Hash specialization for smart asset handles
namespace std {
    template<typename HandleType, typename ResourceType>
    struct hash<SmartAssetHandle<HandleType, ResourceType>> {
        size_t operator()(const SmartAssetHandle<HandleType, ResourceType>& smartHandle) const {
            return hash<HandleType>()(smartHandle.handle());
        }
    };
}