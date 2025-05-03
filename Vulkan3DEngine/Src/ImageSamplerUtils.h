#pragma once
#include "AssetLib.h"
#include "Settings.h"
#include "ImageSampler.h"
#include "Engine.h"

namespace ImageSamplerUtils {
    // Convert AssetLib sampler description to SamplerConfig with settings clamping
    SamplerConfig createSamplerConfig(
        const AssetLib::SamplerDescription& samplerDesc
    ) {
        const Settings& settings = Engine::get().settings();

        // Convert filter modes
        VkFilter magFilter = samplerDesc.magFilter == AssetLib::SamplerDescription::Filter::Linear ?
            VK_FILTER_LINEAR : VK_FILTER_NEAREST;

        VkFilter minFilter = samplerDesc.minFilter == AssetLib::SamplerDescription::Filter::Linear ?
            VK_FILTER_LINEAR : VK_FILTER_NEAREST;

        // Convert mipmap mode based on settings
        VkSamplerMipmapMode mipmapMode = settings.getMipmapMode() == Settings::MipmapMode::Linear ?
            VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;

        // Convert address modes
        VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        switch (samplerDesc.addressModeU) {
        case AssetLib::SamplerDescription::AddressMode::Repeat:
            addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; break;
        case AssetLib::SamplerDescription::AddressMode::MirroredRepeat:
            addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; break;
        case AssetLib::SamplerDescription::AddressMode::ClampToEdge:
            addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break;
        case AssetLib::SamplerDescription::AddressMode::ClampToBorder:
            addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; break;
        }

        VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        switch (samplerDesc.addressModeV) {
        case AssetLib::SamplerDescription::AddressMode::Repeat:
            addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT; break;
        case AssetLib::SamplerDescription::AddressMode::MirroredRepeat:
            addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; break;
        case AssetLib::SamplerDescription::AddressMode::ClampToEdge:
            addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break;
        case AssetLib::SamplerDescription::AddressMode::ClampToBorder:
            addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; break;
        }

        VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        switch (samplerDesc.addressModeW) {
        case AssetLib::SamplerDescription::AddressMode::Repeat:
            addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT; break;
        case AssetLib::SamplerDescription::AddressMode::MirroredRepeat:
            addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; break;
        case AssetLib::SamplerDescription::AddressMode::ClampToEdge:
            addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break;
        case AssetLib::SamplerDescription::AddressMode::ClampToBorder:
            addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; break;
        }

        // Handle texture filtering mode based on global settings
        VkBool32 anisotropyEnable = VK_FALSE;
        float maxAnisotropy = 1.0f;

        // Apply filtering mode based on settings
        switch (settings.getTextureFiltering()) {
        case Settings::TextureFiltering::None:
            // Force nearest filtering regardless of material settings
            magFilter = VK_FILTER_NEAREST;
            minFilter = VK_FILTER_NEAREST;
            mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;

        case Settings::TextureFiltering::Bilinear:
            // Keep material's filtering choice but ensure no anisotropy
            anisotropyEnable = VK_FALSE;
            break;

        case Settings::TextureFiltering::Trilinear:
            // Use linear filtering for everything
            magFilter = VK_FILTER_LINEAR;
            minFilter = VK_FILTER_LINEAR;
            mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            anisotropyEnable = VK_FALSE;
            break;

        case Settings::TextureFiltering::Anisotropic:
            // Enable anisotropy if supported by hardware
            if (settings.isAnisotropySupported()) {
                anisotropyEnable = VK_TRUE;
                // Use the user-specified anisotropy level (already clamped to hardware maximum)
                maxAnisotropy = settings.getCurrentAnisotropyLevel();
            }
            // Use linear filtering for everything
            magFilter = VK_FILTER_LINEAR;
            minFilter = VK_FILTER_LINEAR;
            mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        }

        // Clamp LOD values to valid ranges
        float minLod = samplerDesc.minLod;
        float maxLod = samplerDesc.maxLod;

        // Create the sampler config with all the parameters
        return SamplerConfig(
            magFilter,                    // magFilter
            minFilter,                    // minFilter
            mipmapMode,                   // mipmapMode - now uses settings
            addressModeU,                 // addressModeU
            addressModeV,                 // addressModeV  
            addressModeW,                 // addressModeW
            0.0f,                         // mipLodBias
            anisotropyEnable,             // anisotropyEnable
            maxAnisotropy,                // maxAnisotropy
            VK_FALSE,                     // compareEnable
            VK_COMPARE_OP_ALWAYS,         // compareOp
            minLod,                       // minLod
            maxLod,                       // maxLod
            VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, // borderColor
            VK_FALSE                      // unnormalizedCoordinates
        );
    }
}