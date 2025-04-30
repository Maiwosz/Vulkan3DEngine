#include "MaterialManager.h"

MaterialManager::MaterialManager(ImageSamplerManager& samplerManager)
    : m_samplerManager(samplerManager)
{
}

MaterialHandle MaterialManager::cacheMaterial(const std::string& filename, std::unique_ptr<Material> material)
{
    // Check if the material already exists
    auto it = m_filenameToHandle.find(filename);
    if (it != m_filenameToHandle.end()) {
        // Replace the existing material
        m_materials[it->second] = std::move(material);
        return it->second;
    }

    // Create a new material handle
    MaterialHandle handle(m_nextId++);

    // Configure texture samplers for this material before caching it
    configureTextureSamplers(material.get());

    // Cache the material
    m_materials[handle] = std::move(material);
    m_filenameToHandle[filename] = handle;

    return handle;
}

Material* MaterialManager::get(MaterialHandle handle) const {
    auto it = m_materials.find(handle);
    return (it != m_materials.end()) ? it->second.get() : nullptr;
}

void MaterialManager::freeResource(MaterialHandle handle) {
    if (auto it = m_materials.find(handle); it != m_materials.end()) {
        m_materials.erase(it);

        // Remove from filename mapping
        for (auto& [name, h] : m_filenameToHandle) {
            if (h == handle) {
                m_filenameToHandle.erase(name);
                break;
            }
        }
    }
}

void MaterialManager::configureTextureSamplers(Material* material)
{
    if (!material) return;

    // Process each texture parameter
    for (auto& param : material->parameters()) {
        // Check if this parameter is a texture
        if (auto* textureParam = std::get_if<Material::TextureParam>(&param.value)) {
            // Convert AssetLib sampler description to Vulkan SamplerConfig
            SamplerConfig samplerConfig = convertSamplerDescription(textureParam->sampler);

            // Get or create the sampler from the ImageSamplerManager
            VkSampler sampler = m_samplerManager.getSampler(samplerConfig);

            // Store the sampler handle in the texture parameter
            textureParam->samplerHandle = sampler;
        }
    }
}

SamplerConfig MaterialManager::convertSamplerDescription(const AssetLib::SamplerDescription& desc)
{
    SamplerConfig config;

    // Convert filter settings
    config.magFilter = desc.magFilter == AssetLib::SamplerFilter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    config.minFilter = desc.minFilter == AssetLib::SamplerFilter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

    // Convert address modes
    auto convertAddressMode = [](AssetLib::SamplerAddressMode mode) -> VkSamplerAddressMode {
        switch (mode) {
        case AssetLib::SamplerAddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case AssetLib::SamplerAddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case AssetLib::SamplerAddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case AssetLib::SamplerAddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
        };

    config.addressModeU = convertAddressMode(desc.addressModeU);
    config.addressModeV = convertAddressMode(desc.addressModeV);
    config.addressModeW = convertAddressMode(desc.addressModeW);

    // Set anisotropy if it's greater than 1.0
    if (desc.anisotropy > 1.0f) {
        config.anisotropyEnable = VK_TRUE;
        config.maxAnisotropy = desc.anisotropy;
    }

    // Set LOD values
    config.minLod = desc.minLod;
    config.maxLod = desc.maxLod;

    return config;
}

