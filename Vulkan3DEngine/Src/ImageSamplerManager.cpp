#include "ImageSamplerManager.h"
#include <stdexcept>

ImageSamplerManager::ImageSamplerManager(const LogicalDevice& device)
    : m_device(device) {
    // Reserve some space to avoid frequent reallocations
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
    // This method should be called under write lock

    try {
        SamplerHandle newHandle(m_nextHandleId++);

        // Create new sampler entry
        m_samplers.emplace_back(m_device, config);

        // Map config to handle for fast lookup
        m_configToHandle[config] = newHandle;

        return newHandle;
    }
    catch (const std::exception&) {
        // If sampler creation failed, restore handle ID
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