#include "TextureManager.h"
#include "VramManager.h"
#include "LogicalDevice.h"
#include "Image.h"
#include <stdexcept>
#include <spdlog/spdlog.h>

TextureManager::TextureManager(const LogicalDevice& device, VramManager& vramManager)
    : m_device(device),
    m_vramManager(vramManager) {
    SPDLOG_INFO("TextureHandler initialized");
}

TextureManager::~TextureManager() {
    clear();
}

bool TextureManager::prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) {
    // Validate that this is indeed a texture asset
    if (handle.type != AssetType::Texture || data.header.assetType != AssetLib::AssetType::Texture) {
        SPDLOG_ERROR("Attempted to prepare non-texture asset as texture: {}", handle.filename);
        return false;
    }

    // Default values for model textures
    AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB;
    Graphics::ImageUsage usage = Graphics::ImageUsage::TransferDst | Graphics::ImageUsage::Sampled;
    Settings::MsaaSampleCount samples = Settings::MsaaSampleCount::Samples1;

    // Check if we have dependencies with configuration
    auto dependencies = getDependencies(handle, data);
    if (!dependencies.empty()) {
        // Look for the configuration with settings
        for (const auto& dep : dependencies) {
            if (dep.configuration.has_value()) {
                colorSpace = getColorSpaceFromConfig(dep.configuration);
                usage = getUsageFromConfig(dep.configuration);
                samples = getSamplesFromConfig(dep.configuration);
                break;
            }
        }
    }

    // Create the texture with the appropriate parameters
    TextureHandle textureHandle = createTexture(handle.filename, data, colorSpace, usage, samples);

    // Check if creation was successful
    if (!textureHandle.isValid()) {
        SPDLOG_ERROR("Failed to prepare texture asset: {}", handle.filename);
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_filenameToHandle[handle.filename] = textureHandle;

    SPDLOG_INFO("Successfully prepared texture asset: {}", handle.filename);
    return true;
}

void TextureManager::unloadAsset(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto handleIt = m_filenameToHandle.find(filename);
    if (handleIt != m_filenameToHandle.end()) {
        TextureHandle handle = handleIt->second;

        // Find the texture info
        auto textureIt = m_textures.find(handle.id);
        if (textureIt != m_textures.end()) {
            // Free VRAM resources
            m_vramManager.freeResource(textureIt->second.vramHandle);

            // Clean up Vulkan image view
            if (textureIt->second.imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device.get(), textureIt->second.imageView, nullptr);
            }

            // Remove texture info entry
            m_textures.erase(textureIt);
        }

        // Remove filename mapping
        m_filenameToHandle.erase(handleIt);

        SPDLOG_INFO("Unloaded texture asset: {}", filename);
    }
}

bool TextureManager::isAssetReady(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_filenameToHandle.find(filename);
    if (it == m_filenameToHandle.end()) {
        return false;
    }

    // Check if the texture actually exists in the textures map
    return m_textures.find(it->second.id) != m_textures.end();
}

uint64_t TextureManager::getAssetSize(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto handleIt = m_filenameToHandle.find(filename);
    if (handleIt == m_filenameToHandle.end()) {
        return 0;
    }

    auto textureIt = m_textures.find(handleIt->second.id);
    if (textureIt == m_textures.end()) {
        return 0;
    }

    const TextureMetadata& info = textureIt->second;

    // Use VramManager to get the actual allocated size in VRAM
    return m_vramManager.getResourceSize(info.vramHandle);
}

bool TextureManager::isInVram() const {
    return true;
}

std::vector<AssetDependency> TextureManager::getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const {
    // Textures typically don't have dependencies on other assets
    return {};
}

std::any TextureManager::getResourceInternal(const AssetHandle& handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto handleIt = m_filenameToHandle.find(handle.filename);
    if (handleIt == m_filenameToHandle.end()) {
        return std::any();
    }

    auto textureIt = m_textures.find(handleIt->second.id);
    if (textureIt == m_textures.end()) {
        return std::any();
    }

    // Return a copy of the texture info
    return textureIt->second;
}

std::any TextureManager::getHandleInternal(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_filenameToHandle.find(filename);
    if (it == m_filenameToHandle.end()) {
        return TextureHandle{ 0 };
    }

    return it->second;
}

TextureMetadata* TextureManager::getResource(TextureHandle handle) const {
    return findTextureInfo(handle);
}

bool TextureManager::isAssetReady(TextureHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_textures.find(handle.id) != m_textures.end();
}

// Internal helper to find texture info by handle
TextureMetadata* TextureManager::findTextureInfo(TextureHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_textures.find(handle.id);
    if (it != m_textures.end()) {
        // Return pointer to the stored TextureInfo
        // This is safe as long as the texture exists in the manager
        return const_cast<TextureMetadata*>(&it->second);
    }

    return nullptr;
}

VkImageView TextureManager::getImageView(TextureHandle handle) const {
    TextureMetadata* info = findTextureInfo(handle);
    if (info) {
        return info->imageView;
    }

    SPDLOG_ERROR("Attempted to get image view for invalid texture handle: {}", handle.id);
    return VK_NULL_HANDLE;
}

TextureHandle TextureManager::createCustomTexture(
    const std::string& filename,
    const AssetLib::AssetData& data,
    Graphics::ImageUsage usage,
    Settings::MsaaSampleCount samples,
    AssetLib::ColorSpace colorSpace
) {
    return createTexture(filename, data, colorSpace, usage, samples);
}

void TextureManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    SPDLOG_INFO("Clearing all textures");

    // Destroy all textures
    for (auto& [id, info] : m_textures) {
        // Free VRAM resources
        m_vramManager.freeResource(info.vramHandle);

        // Clean up Vulkan image view
        if (info.imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device.get(), info.imageView, nullptr);
        }
    }

    m_textures.clear();
    m_filenameToHandle.clear();
}

TextureHandle TextureManager::createTexture(
    const std::string& filename,
    const AssetLib::AssetData& data,
    AssetLib::ColorSpace colorSpace,
    Graphics::ImageUsage usage,
    Settings::MsaaSampleCount samples
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

        // Image configuration - now using parameters instead of hardcoded values
        Graphics::ImageCreateInfo imageInfo{
            .width = texInfo.width,
            .height = texInfo.height,
            .format = format,
            .usage = usage,           // Using parameter
            .mipLevels = texInfo.mipLevels,
            .samples = samples        // Using parameter
        };

        SPDLOG_INFO("Creating texture {} ({}x{}, {} mip levels, format {}, colorSpace {}, usage {}, samples {})",
            filename,
            texInfo.width, texInfo.height, texInfo.mipLevels,
            static_cast<int>(texInfo.format),
            static_cast<int>(colorSpace),
            static_cast<int>(usage),
            static_cast<int>(samples));

        // Create resource in VRAM
        VramHandle vramTexture = m_vramManager.createImage(
            imageInfo,
            decompressedData.data(),
            texInfo.mips
        );

        if (!vramTexture.isValid()) {
            SPDLOG_ERROR("VRAM manager returned invalid handle for texture {}", filename);
            return TextureHandle{ 0 };
        }

        // Create our texture handle and store the info
        uint32_t id = m_nextId++;
        TextureHandle handle{ id };

        Image* textureImage = m_vramManager.getResource<Image>(vramTexture);

        // Create image view
        VkImageView imageView = textureImage->createView(
            m_device.get(),
            VK_IMAGE_VIEW_TYPE_2D
        );

        std::lock_guard<std::mutex> lock(m_mutex);
        m_textures[id] = {
            .width = texInfo.width,
            .height = texInfo.height,
            .mipLevels = texInfo.mipLevels,
            .format = format,
            .usage = usage,           // Store the usage
            .samples = samples,       // Store the samples
            .vramHandle = vramTexture,
            .imageView = imageView,
            .colorSpace = colorSpace
        };

        return handle;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error creating texture {}: {}", filename, e.what());
        return TextureHandle{ 0 };
    }
    catch (...) {
        SPDLOG_ERROR("Unknown error creating texture {}", filename);
        return TextureHandle{ 0 };
    }
}

AssetLib::ColorSpace TextureManager::getColorSpaceFromConfig(const std::any& config) const {
    try {
        // Check if the configuration is a map
        if (config.type() == typeid(std::unordered_map<std::string, std::any>)) {
            const auto& configMap = std::any_cast<const std::unordered_map<std::string, std::any>&>(config);

            // Look for the color space setting
            auto it = configMap.find("colorSpace");
            if (it != configMap.end() && it->second.type() == typeid(AssetLib::ColorSpace)) {
                return std::any_cast<AssetLib::ColorSpace>(it->second);
            }
        }
    }
    catch (const std::bad_any_cast&) {
        SPDLOG_WARN("Failed to extract color space from dependency configuration");
    }

    // Default to sRGB if not specified or extraction fails
    return AssetLib::ColorSpace::SRGB;
}

Graphics::ImageUsage TextureManager::getUsageFromConfig(const std::any& config) const {
    try {
        // Check if the configuration is a map
        if (config.type() == typeid(std::unordered_map<std::string, std::any>)) {
            const auto& configMap = std::any_cast<const std::unordered_map<std::string, std::any>&>(config);

            // Look for the usage setting
            auto it = configMap.find("usage");
            if (it != configMap.end() && it->second.type() == typeid(Graphics::ImageUsage)) {
                return std::any_cast<Graphics::ImageUsage>(it->second);
            }
        }
    }
    catch (const std::bad_any_cast&) {
        SPDLOG_WARN("Failed to extract usage from dependency configuration");
    }

    // Default to typical model texture usage if not specified
    return Graphics::ImageUsage::TransferDst | Graphics::ImageUsage::Sampled;
}

Settings::MsaaSampleCount TextureManager::getSamplesFromConfig(const std::any& config) const {
    try {
        // Check if the configuration is a map
        if (config.type() == typeid(std::unordered_map<std::string, std::any>)) {
            const auto& configMap = std::any_cast<const std::unordered_map<std::string, std::any>&>(config);

            // Look for the samples setting
            auto it = configMap.find("samples");
            if (it != configMap.end() && it->second.type() == typeid(Settings::MsaaSampleCount)) {
                return std::any_cast<Settings::MsaaSampleCount>(it->second);
            }
        }
    }
    catch (const std::bad_any_cast&) {
        SPDLOG_WARN("Failed to extract samples from dependency configuration");
    }

    // Default to no multisampling for typical textures
    return Settings::MsaaSampleCount::Samples1;
}