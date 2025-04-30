#include "ImageSampler.h"
#include <stdexcept>
#include <functional>

// Konstruktor SamplerConfig
SamplerConfig::SamplerConfig(
    VkFilter magFilter, VkFilter minFilter,
    VkSamplerMipmapMode mipmapMode,
    VkSamplerAddressMode addressModeU,
    VkSamplerAddressMode addressModeV,
    VkSamplerAddressMode addressModeW,
    float mipLodBias,
    VkBool32 anisotropyEnable,
    float maxAnisotropy,
    VkBool32 compareEnable,
    VkCompareOp compareOp,
    float minLod,
    float maxLod,
    VkBorderColor borderColor,
    VkBool32 unnormalizedCoordinates
) : magFilter(magFilter), minFilter(minFilter),
mipmapMode(mipmapMode),
addressModeU(addressModeU), addressModeV(addressModeV), addressModeW(addressModeW),
mipLodBias(mipLodBias),
anisotropyEnable(anisotropyEnable), maxAnisotropy(maxAnisotropy),
compareEnable(compareEnable), compareOp(compareOp),
minLod(minLod), maxLod(maxLod),
borderColor(borderColor),
unnormalizedCoordinates(unnormalizedCoordinates) {
}

// Operator równości
bool SamplerConfig::operator==(const SamplerConfig& other) const {
    return std::tie(magFilter, minFilter, mipmapMode,
        addressModeU, addressModeV, addressModeW,
        mipLodBias, anisotropyEnable, maxAnisotropy,
        compareEnable, compareOp, minLod, maxLod,
        borderColor, unnormalizedCoordinates) ==
        std::tie(other.magFilter, other.minFilter, other.mipmapMode,
            other.addressModeU, other.addressModeV, other.addressModeW,
            other.mipLodBias, other.anisotropyEnable, other.maxAnisotropy,
            other.compareEnable, other.compareOp, other.minLod, other.maxLod,
            other.borderColor, other.unnormalizedCoordinates);
}

// Hashowanie
namespace std {
    size_t hash<SamplerConfig>::operator()(const SamplerConfig& config) const {
        size_t seed = 0;
        auto hash_combine = [&seed](auto val) {
            seed ^= std::hash<decltype(val)>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            };

        hash_combine(config.magFilter);
        hash_combine(config.minFilter);
        hash_combine(config.mipmapMode);
        hash_combine(config.addressModeU);
        hash_combine(config.addressModeV);
        hash_combine(config.addressModeW);
        hash_combine(config.mipLodBias);
        hash_combine(config.anisotropyEnable);
        if (config.anisotropyEnable) hash_combine(config.maxAnisotropy);
        hash_combine(config.compareEnable);
        hash_combine(config.compareOp);
        hash_combine(config.minLod);
        hash_combine(config.maxLod);
        hash_combine(config.borderColor);
        hash_combine(config.unnormalizedCoordinates);
        return seed;
    }
}

// Implementacja ImageSampler
ImageSampler::ImageSampler(const LogicalDevice& device, const SamplerConfig& config)
    : m_device(device) {

    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = config.magFilter;
    samplerInfo.minFilter = config.minFilter;
    samplerInfo.mipmapMode = config.mipmapMode;
    samplerInfo.addressModeU = config.addressModeU;
    samplerInfo.addressModeV = config.addressModeV;
    samplerInfo.addressModeW = config.addressModeW;
    samplerInfo.mipLodBias = config.mipLodBias;
    samplerInfo.anisotropyEnable = config.anisotropyEnable;
    samplerInfo.maxAnisotropy = config.maxAnisotropy;
    samplerInfo.compareEnable = config.compareEnable;
    samplerInfo.compareOp = config.compareOp;
    samplerInfo.minLod = config.minLod;
    samplerInfo.maxLod = config.maxLod;
    samplerInfo.borderColor = config.borderColor;
    samplerInfo.unnormalizedCoordinates = config.unnormalizedCoordinates;

    if (vkCreateSampler(m_device.get(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler!");
    }
}

ImageSampler::~ImageSampler() {
    vkDestroySampler(m_device.get(), m_sampler, nullptr);
}