#include "MaterialResourceFactory.h"
#include <stdexcept>
#include <cassert>
#include "ImageSamplerUtils.h"
#include <spdlog/spdlog.h>

MaterialResourceFactory::MaterialResourceFactory(
    const LogicalDevice& device,
    ShaderManager& shaderManager,
    ImageSamplerManager& samplerManager,
    BufferManager& uniformBufferManager,
    DescriptorAllocator& descriptorAllocator,
    DescriptorLayoutManager& descriptorLayoutManager,
    TextureManager& textureManager
)
    : m_device(device),
    m_shaderManager(shaderManager),
    m_samplerManager(samplerManager),
    m_bufferManager(uniformBufferManager),
    m_descriptorAllocator(descriptorAllocator),
    m_descriptorLayoutManager(descriptorLayoutManager),
    m_textureManager(textureManager) {
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> MaterialResourceFactory::createMaterialDescriptorSet(
    ShaderHandle shaderHandle,
    const std::vector<Material::Parameter>& parameters
) {
    // Create all required uniform buffers
    auto ubos = createMaterialUniformBuffers(shaderHandle, parameters);

    // Create descriptor set with all UBOs and textures
    return createDescriptorSetInternal(shaderHandle, ubos, parameters);
}

std::vector<MaterialResourceFactory::MaterialUBO> MaterialResourceFactory::createMaterialUniformBuffers(
    ShaderHandle shaderHandle,
    const std::vector<Material::Parameter>& parameters
) {
    std::vector<MaterialUBO> result;

    // Get shader metadata to access UBO information
    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);

    // Process all custom buffers in the CUSTOM_DESCRIPTOR_SET
    for (const auto& buffer : metadata.customBuffers) {
        // Only process UBOs (not SSBOs) in the custom descriptor set
        if (buffer.set != ShaderLib::CUSTOM_DESCRIPTOR_SET || !buffer.IsUniformBuffer()) {
            continue;
        }

        // Check if this UBO has any parameters that need to be set
        auto matchingParams = findParametersForUBO(buffer, parameters);

        // Skip UBOs with no matching parameters (they might be read-only or system-managed)
        if (matchingParams.empty()) {
            SPDLOG_DEBUG("Skipping UBO '{}' at binding {} - no matching parameters",
                buffer.name, buffer.binding);
            continue;
        }

        SPDLOG_DEBUG("Creating UBO '{}' at binding {} with {} parameters",
            buffer.name, buffer.binding, matchingParams.size());

        // Create the buffer using smart handle
        auto uboHandle = m_bufferManager.acquireSmartBuffer(buffer);

        // Update buffer with parameter values using BufferWriter
        updateUniformBufferFromParameters(uboHandle, buffer, parameters);

        // Store the created UBO
        MaterialUBO materialUBO;
        materialUBO.binding = buffer.binding;
        materialUBO.bufferHandle = std::move(uboHandle);
        materialUBO.uboInfo = &buffer;

        result.push_back(std::move(materialUBO));
    }

    if (result.empty()) {
        SPDLOG_WARN("No uniform buffers created for shader - this might be intentional if shader only uses textures");
    }
    else {
        SPDLOG_INFO("Created {} uniform buffer(s) for material", result.size());
    }

    return result;
}

std::vector<const Material::Parameter*> MaterialResourceFactory::findParametersForUBO(
    const ShaderLib::BufferObject& uboInfo,
    const std::vector<Material::Parameter>& parameters
) const {
    std::vector<const Material::Parameter*> result;

    // Create a set of variable names in this UBO for fast lookup
    std::unordered_set<std::string> uboVariableNames;
    for (const auto& variable : uboInfo.variables) {
        uboVariableNames.insert(variable.name);
    }

    // Find all parameters that match variables in this UBO
    for (const auto& param : parameters) {
        // Only consider UniformBuffer parameters
        if (param.descriptorType != ShaderLib::DescriptorType::UniformBuffer) {
            continue;
        }

        // Check if parameter belongs to this UBO
        if (uboVariableNames.find(param.name) != uboVariableNames.end()) {
            result.push_back(&param);
        }
    }

    return result;
}

void MaterialResourceFactory::updateUniformBufferFromParameters(
    const SmartHandle<BufferHandle, Buffer>& uboHandle,
    const ShaderLib::BufferObject& uboInfo,
    const std::vector<Material::Parameter>& parameters
) {
    // Create a map of parameter name to parameter for easy lookup
    std::unordered_map<std::string, const Material::Parameter*> paramMap;
    for (const auto& param : parameters) {
        if (param.descriptorType == ShaderLib::DescriptorType::UniformBuffer) {
            paramMap[param.name] = &param;
        }
    }

    // Get mapped writer for the buffer using RAII wrapper
    auto mappedWriter = m_bufferManager.createMappedWriter(uboHandle.handle());

    if (!mappedWriter.isValid()) {
        SPDLOG_ERROR("Failed to map buffer for writing UBO '{}'", uboInfo.name);
        return;
    }

    uint32_t updatedCount = 0;

    // For each variable in the UBO, try to match a parameter
    for (const auto& variable : uboInfo.variables) {
        auto it = paramMap.find(variable.name);
        if (it == paramMap.end()) {
            // No parameter found for this variable - it might have a default value in shader
            SPDLOG_TRACE("No parameter provided for UBO variable '{}'", variable.name);
            continue;
        }

        const Material::Parameter* param = it->second;

        // Get the BufferValue from the parameter
        const auto* bufferValue = std::get_if<ShaderLib::BufferValue>(&param->value);
        if (!bufferValue) {
            SPDLOG_WARN("Parameter '{}' is not a BufferValue", param->name);
            continue;
        }

        // Use BufferWriter to write the value
        if (mappedWriter->write(variable.name, *bufferValue)) {
            updatedCount++;
            SPDLOG_TRACE("Updated variable '{}' in UBO '{}'", variable.name, uboInfo.name);
        }
        else {
            SPDLOG_WARN("Failed to write variable '{}' to UBO '{}'", variable.name, uboInfo.name);
        }
    }

    SPDLOG_DEBUG("Updated {} variables in UBO '{}'", updatedCount, uboInfo.name);

    // mappedWriter automatically unmaps on destruction (RAII)
}

SmartHandle<DescriptorSetHandle, VkDescriptorSet> MaterialResourceFactory::createDescriptorSetInternal(
    ShaderHandle shaderHandle,
    const std::vector<MaterialUBO>& ubos,
    const std::vector<Material::Parameter>& parameters
) {
    // Get shader resources to access the descriptor set layout
    const auto& resources = m_shaderManager.getShaderResources(shaderHandle);

    // Find the layout for set 2 (CUSTOM_DESCRIPTOR_SET)
    auto layoutIt = resources.descriptorLayouts.find(ShaderLib::CUSTOM_DESCRIPTOR_SET);
    if (layoutIt == resources.descriptorLayouts.end()) {
        SPDLOG_WARN("No descriptor layout found for set {} in shader", ShaderLib::CUSTOM_DESCRIPTOR_SET);
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    // Get the descriptor layout
    VkDescriptorSetLayout layout = m_descriptorLayoutManager.get(layoutIt->second);
    if (layout == VK_NULL_HANDLE) {
        SPDLOG_WARN("Invalid descriptor layout for set {}", ShaderLib::CUSTOM_DESCRIPTOR_SET);
        return SmartHandle<DescriptorSetHandle, VkDescriptorSet>();
    }

    // Create descriptor writer
    DescriptorWriter writer(m_device, m_samplerManager, m_bufferManager, m_descriptorAllocator);

    // Bind all UBOs to their respective bindings
    for (const auto& ubo : ubos) {
        if (ubo.bufferHandle.isValid()) {
            SPDLOG_DEBUG("Binding UBO '{}' to binding {}", ubo.uboInfo->name, ubo.binding);
            writer.writeUniformBuffer(ubo.binding, ubo.bufferHandle);
        }
        else {
            SPDLOG_WARN("Invalid UBO handle for binding {}", ubo.binding);
        }
    }

    // Bind textures to their respective bindings
    uint32_t textureCount = 0;
    for (const auto& param : parameters) {
        // Skip non-texture parameters using the new helper function
        if (!ShaderLib::IsTexture(param.descriptorType)) {
            continue;
        }

        // Get texture parameter
        const Material::TextureParam* textureParam = std::get_if<Material::TextureParam>(&param.value);
        if (!textureParam) {
            continue;
        }

        // Check if texture is loaded
        if (!textureParam->textureHandle.isValid()) {
            SPDLOG_WARN("Texture not loaded for parameter '{}'", param.name);
            continue;
        }

        // Get image view from texture manager
        VkImageView imageView = m_textureManager.getImageView(textureParam->textureHandle);
        if (imageView == VK_NULL_HANDLE) {
            SPDLOG_WARN("Invalid image view for texture parameter '{}'", param.name);
            continue;
        }

        // Write combined image sampler to descriptor set
        SPDLOG_DEBUG("Binding texture '{}' to binding {}", param.name, param.binding);
        writer.writeCombinedImageSampler(param.binding, imageView, textureParam->samplerHandle);
        textureCount++;
    }

    SPDLOG_INFO("Creating descriptor set with {} UBO(s) and {} texture(s)", ubos.size(), textureCount);

    // Create descriptor set with all bindings
    return writer.createDescriptorSet(layout);
}