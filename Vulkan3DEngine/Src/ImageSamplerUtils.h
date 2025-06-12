#pragma once
#include "AssetLib.h"
#include "Settings.h"
#include "ImageSampler.h"
#include "Engine.h"

namespace ImageSamplerUtils {
    // Convert AssetLib sampler description to SamplerConfig with settings clamping
    inline VkSamplerAddressMode convertAddressMode(AssetLib::SamplerDescription::AddressMode addressMode) {
        switch (addressMode) {
        case AssetLib::SamplerDescription::AddressMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case AssetLib::SamplerDescription::AddressMode::MirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case AssetLib::SamplerDescription::AddressMode::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case AssetLib::SamplerDescription::AddressMode::ClampToBorder:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT; // Safe default
        }
    }

    inline SamplerConfig createSamplerConfig(
        const AssetLib::SamplerDescription& samplerDesc
    ) {
        // Convert filter modes - keep original material settings
        VkFilter magFilter = samplerDesc.magFilter == AssetLib::SamplerDescription::Filter::Linear ?
            VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        VkFilter minFilter = samplerDesc.minFilter == AssetLib::SamplerDescription::Filter::Linear ?
            VK_FILTER_LINEAR : VK_FILTER_NEAREST;

        // Use default mipmap mode - will be overridden by settings
        VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        // Convert address modes (unchanged)
        VkSamplerAddressMode addressModeU = convertAddressMode(samplerDesc.addressModeU);
        VkSamplerAddressMode addressModeV = convertAddressMode(samplerDesc.addressModeV);
        VkSamplerAddressMode addressModeW = convertAddressMode(samplerDesc.addressModeW);

        // Create config with original material settings
        // Settings will be applied later by SamplerManager
        return SamplerConfig(
            magFilter,
            minFilter,
            mipmapMode,
            addressModeU,
            addressModeV,
            addressModeW,
            0.0f,                                   // mipLodBias
            VK_FALSE,                               // anisotropyEnable - will be set by settings
            1.0f,                                   // maxAnisotropy - will be set by settings
            VK_FALSE,                               // compareEnable
            VK_COMPARE_OP_ALWAYS,                   // compareOp
            samplerDesc.minLod,                     // minLod
            samplerDesc.maxLod,                     // maxLod
            VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,     // borderColor
            VK_FALSE                                // unnormalizedCoordinates
        );
    }
}