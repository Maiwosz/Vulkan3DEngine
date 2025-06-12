#include "ImageSamplerManager.h"
#include <stdexcept>

ImageSamplerManager::ImageSamplerManager(const LogicalDevice& device, const SamplerSettings& settings)
    : m_device(device), m_settings(settings) {
    m_samplers.reserve(64);
}

ImageSamplerManager::~ImageSamplerManager() {
    m_samplers.clear();
    m_configToHandle.clear();
}

SamplerHandle ImageSamplerManager::acquireSampler(const SamplerConfig& config) {
    // First try to find existing sampler (shared read lock)
    {
        std::shared_lock lock(m_mutex);
        SamplerHandle existingHandle = findExistingSampler(config);
        if (existingHandle.isValid()) {
            return existingHandle;
        }
    }

    // Create new sampler (exclusive write lock)
    std::unique_lock lock(m_mutex);

    // Double-check pattern - another thread might have created it
    SamplerHandle existingHandle = findExistingSampler(config);
    if (existingHandle.isValid()) {
        return existingHandle;
    }

    return createNewSampler(config);
}

void ImageSamplerManager::updateSettings(const SamplerSettings& newSettings) {
    std::unique_lock lock(m_mutex);

    if (m_settings == newSettings) {
        return; // No change
    }

    m_settings = newSettings;
    recreateAllSamplers();
}

bool ImageSamplerManager::isDirty(SamplerHandle handle) const {
    std::shared_lock lock(m_mutex);

    if (!handle.isValid()) {
        return false;
    }

    uint32_t index = handle.id - 1;
    if (index >= m_samplers.size()) {
        return false;
    }

    return m_samplers[index].isDirty;
}

void ImageSamplerManager::clearDirty(SamplerHandle handle) {
    std::shared_lock lock(m_mutex);

    if (!handle.isValid()) {
        return;
    }

    uint32_t index = handle.id - 1;
    if (index < m_samplers.size()) {
        m_samplers[index].isDirty = false;
    }
}

ImageSampler* ImageSamplerManager::getResource(SamplerHandle handle) {
    std::shared_lock lock(m_mutex);

    if (!handle.isValid()) {
        return nullptr;
    }

    uint32_t index = handle.id - 1;
    if (index >= m_samplers.size()) {
        return nullptr;
    }

    return &m_samplers[index].sampler;
}

bool ImageSamplerManager::isValid(SamplerHandle handle) const {
    std::shared_lock lock(m_mutex);

    if (!handle.isValid()) {
        return false;
    }

    uint32_t index = handle.id - 1;
    return index < m_samplers.size();
}

void ImageSamplerManager::releaseResource(SamplerHandle handle) {
    // No-op - samplers are kept until manager destruction
    (void)handle; // Suppress unused parameter warning
}

void ImageSamplerManager::addReference(SamplerHandle handle) {
    // No-op - samplers are shared resources, no reference counting needed
    // This manager keeps all samplers until destruction
    (void)handle; // Suppress unused parameter warning
}

void ImageSamplerManager::removeReference(SamplerHandle handle) {
    // No-op - samplers are shared resources, no reference counting needed
    // This manager keeps all samplers until destruction
    (void)handle; // Suppress unused parameter warning
}

SamplerHandle ImageSamplerManager::createNewSampler(const SamplerConfig& config) {
    try {
        SamplerHandle newHandle(m_nextHandleId++);

        // Apply current settings to the config
        SamplerConfig actualConfig = applySamplerSettings(config);

        // Create new sampler entry
        m_samplers.emplace_back(m_device, config, actualConfig);

        // Map original config to handle for fast lookup
        m_configToHandle[config] = newHandle;

        return newHandle;
    }
    catch (const std::exception&) {
        --m_nextHandleId;
        throw;
    }
}

SamplerHandle ImageSamplerManager::findExistingSampler(const SamplerConfig& config) {
    // This method should be called under some lock (read or write)

    auto it = m_configToHandle.find(config);
    if (it != m_configToHandle.end()) {
        return it->second;
    }

    return SamplerHandle{}; // Invalid handle
}

void ImageSamplerManager::recreateAllSamplers() {
    // Clear the config-to-handle mapping as actual configs will change
    m_configToHandle.clear();

    // Recreate all samplers with new settings
    for (auto& entry : m_samplers) {
        SamplerConfig newActualConfig = applySamplerSettings(entry.originalConfig);

        if (!(newActualConfig == entry.actualConfig)) {
            // Config changed, recreate sampler
            entry.sampler = ImageSampler(m_device, newActualConfig);
            entry.actualConfig = newActualConfig;
            entry.isDirty = true;
        }

        // Rebuild mapping with original config
        SamplerHandle handle(static_cast<uint32_t>(&entry - &m_samplers[0]) + 1);
        m_configToHandle[entry.originalConfig] = handle;
    }
}

SamplerConfig ImageSamplerManager::applySamplerSettings(const SamplerConfig& originalConfig) const {
    SamplerConfig result = originalConfig;

    // Apply texture filtering settings
    switch (m_settings.textureFiltering) {
    case Settings::TextureFiltering::None:
        result.magFilter = VK_FILTER_NEAREST;
        result.minFilter = VK_FILTER_NEAREST;
        result.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        result.anisotropyEnable = VK_FALSE;
        result.maxAnisotropy = 1.0f;
        break;

    case Settings::TextureFiltering::Bilinear:
        // Keep original filter settings but disable anisotropy
        result.anisotropyEnable = VK_FALSE;
        result.maxAnisotropy = 1.0f;
        break;

    case Settings::TextureFiltering::Trilinear:
        result.magFilter = VK_FILTER_LINEAR;
        result.minFilter = VK_FILTER_LINEAR;
        result.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        result.anisotropyEnable = VK_FALSE;
        result.maxAnisotropy = 1.0f;
        break;

    case Settings::TextureFiltering::Anisotropic:
        result.magFilter = VK_FILTER_LINEAR;
        result.minFilter = VK_FILTER_LINEAR;
        result.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        if (m_settings.anisotropySupported) {
            result.anisotropyEnable = VK_TRUE;
            result.maxAnisotropy = m_settings.maxAnisotropy;
        }
        break;
    }

    // Apply mipmap mode from settings
    if (m_settings.mipmapMode == Settings::MipmapMode::Nearest) {
        result.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }
    else {
        result.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    return result;
}