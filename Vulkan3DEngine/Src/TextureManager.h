#pragma once
#include "IAssetHandler.h"
#include "Handle.h"
#include "VramManager.h"
#include "AssetLib.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>

// Forward declarations
class LogicalDevice;
class VramManager;
class Image;

struct TextureMetadata {
    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
    Graphics::ImageFormat format;
    Graphics::ImageUsage usage;
    Settings::MsaaSampleCount samples;
    VramHandle vramHandle;
    VkImageView imageView;
    AssetLib::ColorSpace colorSpace;
};

// Typedefs for convenience
using SmartTextureHandle = SmartAssetHandle<TextureHandle, TextureMetadata>;

class TextureManager : public ISmartAssetHandler<TextureHandle, TextureMetadata> {
public:
    TextureManager(const LogicalDevice& device, VramManager& vramManager);
    ~TextureManager() override;

    // IAssetHandler implementation
    bool prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) override;
    void unloadAsset(const std::string& filename) override;
    bool isAssetReady(const std::string& filename) const override;
    uint64_t getAssetSize(const std::string& filename) const override;
    bool isInVram() const override;
    std::vector<AssetDependency> getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const override;
    std::any getResourceInternal(const AssetHandle& handle) const override;
    std::any getHandleInternal(const std::string& filename) const override;

    // ISmartAssetHandler implementation
    TextureMetadata* getResource(TextureHandle handle) const override;
    bool isAssetReady(TextureHandle handle) const override;

    // TextureManager specific methods
    VkImageView getImageView(TextureHandle handle) const;
    VkImageView getImageView(const SmartTextureHandle& smartHandle) const {
        return getImageView(smartHandle.handle());
    }

    // Smart handle factory methods
    SmartTextureHandle loadTexture(const std::string& filename) {
        // This would typically trigger asset loading if not already loaded
        // For now, just return existing or invalid handle
        return getSmartAsset(filename);
    }

    // Create texture with custom parameters for non-model use cases
    TextureHandle createCustomTexture(
        const std::string& filename,
        const AssetLib::AssetData& data,
        Graphics::ImageUsage usage,
        Settings::MsaaSampleCount samples = Settings::MsaaSampleCount::Samples1,
        AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB
    );

    // Clear all resources
    void clear();

private:
    const LogicalDevice& m_device;
    VramManager& m_vramManager;
    mutable std::mutex m_mutex; // For thread safety

    // Maps from filename to texture handle
    std::unordered_map<std::string, TextureHandle> m_filenameToHandle;

    // Maps from texture handle ID to texture info
    std::unordered_map<uint32_t, TextureMetadata> m_textures;

    uint32_t m_nextId = 1;

    // Helper method to create a texture from asset data with specific parameters
    TextureHandle createTexture(
        const std::string& filename,
        const AssetLib::AssetData& data,
        AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB,
        Graphics::ImageUsage usage = Graphics::ImageUsage::TransferDst | Graphics::ImageUsage::Sampled,
        Settings::MsaaSampleCount samples = Settings::MsaaSampleCount::Samples1
    );

    // Gets color space from dependency configuration
    AssetLib::ColorSpace getColorSpaceFromConfig(const std::any& config) const;

    // Gets usage from dependency configuration
    Graphics::ImageUsage getUsageFromConfig(const std::any& config) const;

    // Gets samples from dependency configuration
    Settings::MsaaSampleCount getSamplesFromConfig(const std::any& config) const;

    // Internal helper to find texture info by handle
    TextureMetadata* findTextureInfo(TextureHandle handle) const;
};