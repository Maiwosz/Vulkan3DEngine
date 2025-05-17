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

class TextureManager : public IAssetHandler {
public:
    struct TextureInfo {
        uint32_t width;
        uint32_t height;
        uint32_t mipLevels;
        Graphics::ImageFormat format;
        VramHandle vramHandle;
        VkImageView imageView;
        AssetLib::ColorSpace colorSpace;
    };

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

    VkImageView getImageView(TextureHandle handle) const;

    // Clear all resources
    void clear();

private:
    const LogicalDevice& m_device;
    VramManager& m_vramManager;
    mutable std::mutex m_mutex; // For thread safety

    // Maps from filename to texture handle
    std::unordered_map<std::string, TextureHandle> m_filenameToHandle;

    // Maps from texture handle ID to texture info
    std::unordered_map<uint32_t, TextureInfo> m_textures;

    uint32_t m_nextId = 1;

    // Helper method to create a texture from asset data with specific color space
    TextureHandle createTexture(
        const std::string& filename,
        const AssetLib::AssetData& data,
        AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB
    );

    // Gets color space from dependency configuration
    AssetLib::ColorSpace getColorSpaceFromConfig(const std::any& config) const;
};