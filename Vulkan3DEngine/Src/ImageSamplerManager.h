#pragma once
#include "ImageSampler.h"
#include <unordered_map>
#include <shared_mutex>

class ImageSamplerManager {
public:
    explicit ImageSamplerManager(const LogicalDevice& device);
    ~ImageSamplerManager();

    VkSampler getSampler(const SamplerConfig& config);
    void destroySampler(const SamplerConfig& config);
    void destroyAllSamplers();

private:
    const LogicalDevice& m_device;
    std::unordered_map<SamplerConfig, ImageSampler> m_samplers;
    mutable std::shared_mutex m_mutex;
};