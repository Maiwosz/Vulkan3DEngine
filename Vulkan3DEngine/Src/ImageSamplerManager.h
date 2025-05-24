#pragma once
#include "ImageSampler.h"
#include "Handle.h"
#include "IResourceManager.h"
#include <unordered_map>
#include <shared_mutex>

class ImageSamplerManager : public IResourceManager<SamplerHandle, ImageSampler> {
public:
    explicit ImageSamplerManager(const LogicalDevice& device);
    ~ImageSamplerManager();

    // Main public interface - get sampler by config (creates if needed)
    SamplerHandle acquireSampler(const SamplerConfig& config);

    // IResourceManager interface implementation (WSZYSTKIE metody wymagane)
    ImageSampler* getResource(SamplerHandle handle) override;
    bool isValid(SamplerHandle handle) const override;
    void releaseResource(SamplerHandle handle) override;
    void addReference(SamplerHandle handle) override;
    void removeReference(SamplerHandle handle) override;
private:
    struct SamplerEntry {
        ImageSampler sampler;
        SamplerConfig config;

        // Explicit constructor
        SamplerEntry(const LogicalDevice& device, const SamplerConfig& cfg)
            : sampler(device, cfg), config(cfg) {
        }

        // Delete copy constructor and assignment operator since ImageSampler is non-copyable
        SamplerEntry(const SamplerEntry&) = delete;
        SamplerEntry& operator=(const SamplerEntry&) = delete;

        // Add move constructor and assignment operator
        SamplerEntry(SamplerEntry&& other) noexcept
            : sampler(std::move(other.sampler)), config(std::move(other.config)) {
        }

        SamplerEntry& operator=(SamplerEntry&& other) noexcept {
            if (this != &other) {
                sampler = std::move(other.sampler);
                config = std::move(other.config);
            }
            return *this;
        }
    };

    // Private helper methods
    SamplerHandle createNewSampler(const SamplerConfig& config);
    SamplerHandle findExistingSampler(const SamplerConfig& config);

    const LogicalDevice& m_device;
    std::vector<SamplerEntry> m_samplers;
    std::unordered_map<SamplerConfig, SamplerHandle> m_configToHandle;
    uint32_t m_nextHandleId = 1;
    mutable std::shared_mutex m_mutex;
};