#include "ImageSamplerManager.h"
#include <stdexcept>

ImageSamplerManager::ImageSamplerManager(const LogicalDevice& device)
    : m_device(device) {
}

ImageSamplerManager::~ImageSamplerManager() {
    destroyAllSamplers();
}

VkSampler ImageSamplerManager::getSampler(const SamplerConfig& config) {
    // Odczyt współdzielony
    {
        std::shared_lock lock(m_mutex);
        auto it = m_samplers.find(config);
        if (it != m_samplers.end()) {
            return it->second.handle();
        }
    }

    // Zapis wyłączny
    std::unique_lock lock(m_mutex);
    auto [iterator, inserted] = m_samplers.try_emplace(config, m_device, config);
    return iterator->second.handle();
}

void ImageSamplerManager::destroySampler(const SamplerConfig& config) {
    std::unique_lock lock(m_mutex);
    m_samplers.erase(config);
}

void ImageSamplerManager::destroyAllSamplers() {
    std::unique_lock lock(m_mutex);
    m_samplers.clear();
}