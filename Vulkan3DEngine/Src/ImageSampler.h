#pragma once
#include <vulkan/vulkan.h>
#include <functional>
#include "LogicalDevice.h"

struct SamplerConfig {
    VkFilter magFilter;
    VkFilter minFilter;
    VkSamplerMipmapMode mipmapMode;
    VkSamplerAddressMode addressModeU;
    VkSamplerAddressMode addressModeV;
    VkSamplerAddressMode addressModeW;
    float mipLodBias;
    VkBool32 anisotropyEnable;
    float maxAnisotropy;
    VkBool32 compareEnable;
    VkCompareOp compareOp;
    float minLod;
    float maxLod;
    VkBorderColor borderColor;
    VkBool32 unnormalizedCoordinates;

    SamplerConfig(
        VkFilter magFilter = VK_FILTER_LINEAR,
        VkFilter minFilter = VK_FILTER_LINEAR,
        VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        float mipLodBias = 0.0f,
        VkBool32 anisotropyEnable = VK_FALSE,
        float maxAnisotropy = 1.0f,
        VkBool32 compareEnable = VK_FALSE,
        VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS,
        float minLod = 0.0f,
        float maxLod = VK_LOD_CLAMP_NONE,
        VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        VkBool32 unnormalizedCoordinates = VK_FALSE
    );

    bool operator==(const SamplerConfig& other) const;
};

namespace std {
    template<> struct hash<SamplerConfig> {
        size_t operator()(const SamplerConfig& config) const;
    };
}

class ImageSampler {
public:
    ImageSampler(const LogicalDevice& device, const SamplerConfig& config);
    ~ImageSampler();

    ImageSampler(const ImageSampler&) = delete;
    ImageSampler& operator=(const ImageSampler&) = delete;

    VkSampler handle() const { return m_sampler; }

private:
    const LogicalDevice& m_device;
    VkSampler m_sampler;
};