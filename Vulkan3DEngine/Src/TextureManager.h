#pragma once
#include "Handle.h"
#include "VramManager.h"
#include "AssetLib.h"
#include <unordered_map>
#include <string>
#include <vector>

// Forward declarations
class VramManager;

class TextureManager {
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

    explicit TextureManager(
        const LogicalDevice& device,
        VramManager& vramManager
    );
    ~TextureManager();

    // Create a texture from asset data
    TextureHandle createTexture(
        const AssetLib::AssetData& data,
        AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB
    );

    // Create a texture from raw parameters
    TextureHandle createTexture(
        uint32_t width,
        uint32_t height,
        Graphics::ImageFormat format,
        const void* data,
        size_t dataSize,
        uint32_t mipLevels = 1,
        AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB
    );

    const TextureInfo* getTextureInfo(TextureHandle handle) const;
    const VkImageView getVkImageView(TextureHandle handle) const;
    VramHandle getVramHandle(TextureHandle handle) const;

    void destroyTexture(TextureHandle handle);

    void clear();

private:
    const LogicalDevice& m_device;
    VramManager& m_vramManager;
    std::unordered_map<uint32_t, TextureInfo> m_textures;
    uint32_t m_nextId = 1;
};