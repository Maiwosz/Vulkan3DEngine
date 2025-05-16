#include "TextureManager.h"
#include <stdexcept>
#include <spdlog/spdlog.h>


TextureManager::TextureManager(const LogicalDevice& device, VramManager& vramManager)
    : m_device(device),
    m_vramManager(vramManager) {
    SPDLOG_INFO("TextureManager initialized");
}

TextureManager::~TextureManager() {
    clear();
}

TextureHandle TextureManager::createTexture(
    const AssetLib::AssetData& data,
    AssetLib::ColorSpace colorSpace
) {
    try {
        auto [texInfo, decompressedData] = AssetLib::ReadTexture(data);

        // Map texture format based on color space
        Graphics::ImageFormat format;
        switch (texInfo.format) {
        case AssetLib::TextureFormat::RGBA8:
            // Choose sRGB or linear format based on colorSpace parameter
            format = (colorSpace == AssetLib::ColorSpace::SRGB) ?
                Graphics::ImageFormat::R8G8B8A8_SRGB :
                Graphics::ImageFormat::R8G8B8A8_UNORM;
            break;
        case AssetLib::TextureFormat::BC7:
            format = (colorSpace == AssetLib::ColorSpace::SRGB) ?
                Graphics::ImageFormat::BC7_SRGB :
                Graphics::ImageFormat::BC7_UNORM;
            break;
        default:
            SPDLOG_ERROR("Unsupported texture format: {}", static_cast<int>(texInfo.format));
            throw std::runtime_error("Unsupported texture format");
        }

        // Image configuration
        Graphics::ImageCreateInfo imageInfo{
            .width = texInfo.width,
            .height = texInfo.height,
            .format = format,
            .usage = Graphics::ImageUsage::TransferDst | Graphics::ImageUsage::Sampled,
            .mipLevels = texInfo.mipLevels,
            .samples = Settings::MsaaSampleCount::Samples1
        };

        SPDLOG_INFO("Creating texture ({}x{}, {} mip levels, format {}, colorSpace {})",
            texInfo.width, texInfo.height, texInfo.mipLevels,
            static_cast<int>(texInfo.format),
            static_cast<int>(colorSpace));

        // Create resource in VRAM
        VramHandle vramTexture = m_vramManager.createImage(
            imageInfo,
            decompressedData.data(),
            texInfo.mips
        );

        if (!vramTexture.isValid()) {
            SPDLOG_ERROR("VRAM manager returned invalid handle for texture");
            return TextureHandle{ 0 };
        }

        // Create our texture handle and store the info
        uint32_t id = m_nextId++;
        TextureHandle handle{ id };

        Image* textureImage = m_vramManager.getResource<Image>(vramTexture);

        m_textures[id] = {
            .width = texInfo.width,
            .height = texInfo.height,
            .mipLevels = texInfo.mipLevels,
            .format = format,
            .vramHandle = vramTexture,
            .imageView = textureImage->createView(
                m_device.get(),
                VK_IMAGE_VIEW_TYPE_2D
            ),
            .colorSpace = colorSpace  // Store the color space
        };

        return handle;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error creating texture: {}", e.what());
        return TextureHandle{ 0 };
    }
    catch (...) {
        SPDLOG_ERROR("Unknown error creating texture");
        return TextureHandle{ 0 };
    }
}

// Also update the raw texture creation method to accept colorSpace
TextureHandle TextureManager::createTexture(
    uint32_t width,
    uint32_t height,
    Graphics::ImageFormat format,
    const void* data,
    size_t dataSize,
    uint32_t mipLevels,
    AssetLib::ColorSpace colorSpace
) {
    try {
        // Image configuration
        Graphics::ImageCreateInfo imageInfo{
            .width = width,
            .height = height,
            .format = format,
            .usage = Graphics::ImageUsage::TransferDst | Graphics::ImageUsage::Sampled,
            .mipLevels = mipLevels,
            .samples = Settings::MsaaSampleCount::Samples1
        };

        SPDLOG_INFO("Creating raw texture ({}x{}, {} mip levels, colorSpace {})",
            width, height, mipLevels, static_cast<int>(colorSpace));

        // Create resource in VRAM
        VramHandle vramTexture = m_vramManager.createImage(
            imageInfo,
            data,
            {} // No mip data for raw textures
        );

        if (!vramTexture.isValid()) {
            SPDLOG_ERROR("VRAM manager returned invalid handle for raw texture");
            return TextureHandle{ 0 };
        }

        // Create our texture handle and store the info
        uint32_t id = m_nextId++;
        TextureHandle handle{ id };

        Image* textureImage = m_vramManager.getResource<Image>(vramTexture);

        m_textures[id] = {
            .width = width,
            .height = height,
            .mipLevels = mipLevels,
            .format = format,
            .vramHandle = vramTexture,
            .imageView = textureImage->createView(
                m_device.get(),
                VK_IMAGE_VIEW_TYPE_2D
            ),
            .colorSpace = colorSpace
        };

        return handle;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error creating raw texture: {}", e.what());
        return TextureHandle{ 0 };
    }
    catch (...) {
        SPDLOG_ERROR("Unknown error creating raw texture");
        return TextureHandle{ 0 };
    }
}

const TextureManager::TextureInfo* TextureManager::getTextureInfo(TextureHandle handle) const {
    auto it = m_textures.find(handle.id);
    if (it == m_textures.end()) {
        return nullptr;
    }
    return &it->second;
}

const VkImageView TextureManager::getVkImageView(TextureHandle handle) const
{
    auto it = m_textures.find(handle.id);
    if (it == m_textures.end()) {
        return VkImageView{ 0 };
    }
    return it->second.imageView;
}

VramHandle TextureManager::getVramHandle(TextureHandle handle) const {
    auto it = m_textures.find(handle.id);
    if (it == m_textures.end()) {
        return VramHandle{ 0 };
    }
    return it->second.vramHandle;
}

void TextureManager::destroyTexture(TextureHandle handle) {
    auto it = m_textures.find(handle.id);
    if (it != m_textures.end()) {
        SPDLOG_INFO("Destroying texture #{}", handle.id);
        m_vramManager.freeResource(it->second.vramHandle);
        m_textures.erase(it);
    }
}

void TextureManager::clear() {
    SPDLOG_INFO("Clearing all textures");
    for (auto& [id, info] : m_textures) {
        m_vramManager.freeResource(info.vramHandle);
    }
    m_textures.clear();
}