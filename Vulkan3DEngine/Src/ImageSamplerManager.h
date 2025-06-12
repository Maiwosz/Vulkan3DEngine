#pragma once
#include "ImageSampler.h"
#include "Handle.h"
#include "IResourceManager.h"
#include <unordered_map>
#include <shared_mutex>
#include "Settings.h"

struct SamplerSettings{
    Settings::TextureFiltering textureFiltering = Settings::TextureFiltering::Anisotropic;
    Settings::MipmapMode mipmapMode = Settings::MipmapMode::Linear;
    float maxAnisotropy = 8.0f;
    bool anisotropySupported = true;

    bool operator==(const SamplerSettings& other) const {
        return textureFiltering == other.textureFiltering &&
               mipmapMode == other.mipmapMode &&
               maxAnisotropy == other.maxAnisotropy &&
               anisotropySupported == other.anisotropySupported;
    }
};

class ImageSamplerManager : public IResourceManager<SamplerHandle, ImageSampler> {
public:
    explicit ImageSamplerManager(const LogicalDevice& device, const SamplerSettings& settings);
    ~ImageSamplerManager();

    // Main public interface - get sampler by config (creates if needed)
    SamplerHandle acquireSampler(const SamplerConfig& config);

    // Update settings and recreate all samplers
    void updateSettings(const SamplerSettings& newSettings);

    // Check if sampler is dirty (needs descriptor update)
    bool isDirty(SamplerHandle handle) const;
    void clearDirty(SamplerHandle handle);

    // IResourceManager interface implementation
    ImageSampler* getResource(SamplerHandle handle) override;
    bool isValid(SamplerHandle handle) const override;
    void releaseResource(SamplerHandle handle) override;
    void addReference(SamplerHandle handle) override;
    void removeReference(SamplerHandle handle) override;

private:
    struct SamplerEntry {
        ImageSampler sampler;
        SamplerConfig originalConfig;  // Original config from asset
        SamplerConfig actualConfig;    // Config with applied settings
        bool isDirty = false;

        SamplerEntry(const LogicalDevice& device,
            const SamplerConfig& origConfig,
            const SamplerConfig& actualConfig)
            : sampler(device, actualConfig),
            originalConfig(origConfig),
            actualConfig(actualConfig) {
        }

        // Move semantics
        SamplerEntry(SamplerEntry&& other) noexcept
            : sampler(std::move(other.sampler)),
            originalConfig(std::move(other.originalConfig)),
            actualConfig(std::move(other.actualConfig)),
            isDirty(other.isDirty) {
        }

        SamplerEntry& operator=(SamplerEntry&& other) noexcept {
            if (this != &other) {
                sampler = std::move(other.sampler);
                originalConfig = std::move(other.originalConfig);
                actualConfig = std::move(other.actualConfig);
                isDirty = other.isDirty;
            }
            return *this;
        }

        // Delete copy operations
        SamplerEntry(const SamplerEntry&) = delete;
        SamplerEntry& operator=(const SamplerEntry&) = delete;
    };

    // Helper methods
    SamplerHandle createNewSampler(const SamplerConfig& config);
    SamplerHandle findExistingSampler(const SamplerConfig& config);
    SamplerConfig applySamplerSettings(const SamplerConfig& originalConfig) const;
    void recreateAllSamplers();

    const LogicalDevice& m_device;
    SamplerSettings m_settings;
    std::vector<SamplerEntry> m_samplers;
    std::unordered_map<SamplerConfig, SamplerHandle> m_configToHandle;
    uint32_t m_nextHandleId = 1;
    mutable std::shared_mutex m_mutex;
};