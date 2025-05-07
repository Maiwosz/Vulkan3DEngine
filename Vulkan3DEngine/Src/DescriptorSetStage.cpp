#include "DescriptorSetStage.h"
#include "ShaderModuleManager.h"
#include "Material.h"
#include "Buffer.h"

DescriptorSetStage::DescriptorSetStage(
    Renderer& renderer
)
    : m_renderer(renderer),
    m_shaderManager(renderer.shaderModuleManager()),
    m_materialManager(renderer.materialManager()),
    m_uniformBufferManager(renderer.uniformBufferManager()),
    m_descriptorAllocator(renderer.descriptorAllocator()),
    m_vramManager(renderer.vramManager()),
    m_layoutManager(renderer.descriptorLayoutManager()),
    m_writer()
{
    SPDLOG_INFO("Initializing DescriptorSetStage");
}

void DescriptorSetStage::process(std::shared_ptr<RenderOrder> order) {
    if (!order) {
        SPDLOG_WARN("Attempted to process null render order");
        return;
    }

    // Process based on order type
    switch (order->getType()) {
    case RenderOrderType::Mesh: {
        auto meshOrder = std::static_pointer_cast<MeshRenderOrder>(order);
        processMeshOrder(meshOrder);
        break;
    }
    case RenderOrderType::Light: {
        auto lightOrder = std::static_pointer_cast<LightRenderOrder>(order);
        processLightOrder(lightOrder);
        break;
    }
    case RenderOrderType::Camera: {
        auto cameraOrder = std::static_pointer_cast<CameraRenderOrder>(order);
        processCameraOrder(cameraOrder);
        break;
    }
    default:
        SPDLOG_WARN("Unknown render order type: {}", static_cast<int>(order->getType()));
        break;
    }

    // Forward the order to the next stage
    forwardToNextStage(order);
}

void DescriptorSetStage::processMeshOrder(std::shared_ptr<MeshRenderOrder> order) {
    // Get the material
    Material* material = m_materialManager.get(order->materialHandle);
    if (!material) {
        SPDLOG_ERROR("Invalid material handle for entity {}", order->entity.id);
        return; // Invalid material, can't continue
    }

    // Get shader handle from material
    ShaderHandle shader = material->shader();
    if (!m_shaderManager.isShaderValid(shader)) {
        SPDLOG_ERROR("Invalid shader for material '{}', entity {}", material->name(), order->entity.id);
        return; // Invalid shader, can't continue
    }

    // Create descriptor sets for each set binding (0, 1, 2)
    createGlobalDescriptorSet(order, shader);
    createObjectDescriptorSet(order, shader);
    createMaterialDescriptorSet(order, shader, material);
}

void DescriptorSetStage::processLightOrder(std::shared_ptr<LightRenderOrder> order) {
    // For light orders, we typically don't need separate descriptor sets
    // They are usually included in the global UBO
    // But we could add specific processing here if needed
}

void DescriptorSetStage::processCameraOrder(std::shared_ptr<CameraRenderOrder> order) {
    // For camera orders, we typically update the global UBO
    // They are usually included in the global UBO view/projection matrices
    // But we could add specific processing here if needed
}

void DescriptorSetStage::createGlobalDescriptorSet(std::shared_ptr<MeshRenderOrder> order, ShaderHandle shader) {
    // Get shader resources
    const auto& shaderResources = m_shaderManager.getShaderResources(shader);

    // Find descriptor layout for set 0
    auto layoutIt = shaderResources.descriptorLayouts.find(0);
    if (layoutIt == shaderResources.descriptorLayouts.end()) {
        SPDLOG_WARN("No descriptor layout for set 0 found for entity {}", order->entity.id);
        return; // No layout for set 0
    }

    // Allocate descriptor set for set 0 (Global UBO)
    VkDescriptorSetLayout layout = m_layoutManager.get(layoutIt->second);
    VkDescriptorSet descriptorSet = m_descriptorAllocator.allocate(layout);

    if (descriptorSet == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Failed to allocate global descriptor set for entity {}", order->entity.id);
        return;
    }

    // Get the global UBO buffer
    Buffer* globalBuffer = m_uniformBufferManager.getBuffer(order->globalUBOHandle);
    if (!globalBuffer) {
        SPDLOG_ERROR("Invalid global UBO buffer for entity {}", order->entity.id);
        return; // Invalid global UBO
    }

    // Write descriptor set for global UBO
    m_writer.clear();
    m_writer.writeBuffer(0, globalBuffer->get(), globalBuffer->getAllocatedSize(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_writer.updateSet(m_renderer.vulkanContext().logical().get(), descriptorSet);

    // Store the descriptor set in the order
    order->globalDescriptorSet = descriptorSet;
}

void DescriptorSetStage::createObjectDescriptorSet(std::shared_ptr<MeshRenderOrder> order, ShaderHandle shader) {
    // Get shader resources
    const auto& shaderResources = m_shaderManager.getShaderResources(shader);

    // Find descriptor layout for set 1
    auto layoutIt = shaderResources.descriptorLayouts.find(1);
    if (layoutIt == shaderResources.descriptorLayouts.end()) {
        SPDLOG_WARN("No descriptor layout for set 1 found for entity {}", order->entity.id);
        return; // No layout for set 1
    }

    // Allocate descriptor set for set 1 (Object UBO)
    VkDescriptorSetLayout layout = m_layoutManager.get(layoutIt->second);
    VkDescriptorSet descriptorSet = m_descriptorAllocator.allocate(layout);

    if (descriptorSet == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Failed to allocate object descriptor set for entity {}", order->entity.id);
        return;
    }

    // Get the object UBO buffer
    Buffer* objectBuffer = m_uniformBufferManager.getBuffer(order->objectUBOHandle);
    if (!objectBuffer) {
        SPDLOG_ERROR("Invalid object UBO buffer for entity {}", order->entity.id);
        return; // Invalid object UBO
    }

    // Write descriptor set for object UBO
    m_writer.clear();
    m_writer.writeBuffer(0, objectBuffer->get(), objectBuffer->getAllocatedSize(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_writer.updateSet(m_renderer.vulkanContext().logical().get(), descriptorSet);

    // Store the descriptor set in the order
    order->objectDescriptorSet = descriptorSet;
}

void DescriptorSetStage::createMaterialDescriptorSet(std::shared_ptr<MeshRenderOrder> order, ShaderHandle shader, Material* material) {
    // Get shader resources
    const auto& shaderResources = m_shaderManager.getShaderResources(shader);

    // Find descriptor layout for set 2
    auto layoutIt = shaderResources.descriptorLayouts.find(2);
    if (layoutIt == shaderResources.descriptorLayouts.end()) {
        SPDLOG_WARN("No descriptor layout for set 2 found for entity {}", order->entity.id);
        return; // No layout for set 2
    }

    // Allocate descriptor set for set 2 (Material data)
    VkDescriptorSetLayout layout = m_layoutManager.get(layoutIt->second);
    VkDescriptorSet descriptorSet = m_descriptorAllocator.allocate(layout);

    if (descriptorSet == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Failed to allocate material descriptor set for entity {}", order->entity.id);
        return;
    }

    // Get the material UBO buffer
    Buffer* materialBuffer = m_uniformBufferManager.getBuffer(order->materialUBOHandle);
    if (!materialBuffer) {
        SPDLOG_ERROR("Invalid material UBO buffer for entity {}", order->entity.id);
        return; // Invalid material UBO
    }

    // Clear writer for new descriptor set
    m_writer.clear();

    // Write descriptor set for material UBO at binding 0
    m_writer.writeBuffer(0, materialBuffer->get(), materialBuffer->getAllocatedSize(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // Write texture descriptors if available
    for (const auto& param : material->parameters()) {
        // Only process texture parameters
        if (std::holds_alternative<Material::TextureParam>(param.value) &&
            param.descriptorType == AssetLib::DescriptorType::CombinedImageSampler) {

            const auto& textureParam = std::get<Material::TextureParam>(param.value);

            // Skip invalid textures
            if (!textureParam.vramHandle.isValid()) {
                SPDLOG_WARN("Invalid VRAM handle for texture parameter at binding {} in material '{}'",
                    param.binding, material->name());
                continue;
            }

            // Get the Image resource from VramManager
            Image* textureImage = m_vramManager.getResource<Image>(textureParam.vramHandle);
            if (!textureImage) {
                SPDLOG_WARN("Failed to get image resource for texture parameter at binding {} in material '{}'",
                    param.binding, material->name());
                continue;
            }

            // Create image view
            VkImageView view = textureImage->createView(
                m_renderer.vulkanContext().logical().get(),
                VK_IMAGE_VIEW_TYPE_2D
                // Format will use the image's format by default
                // AspectMask will be derived from the format automatically
                // Mip levels will default to VK_REMAINING_MIP_LEVELS
                // Array layers will default to VK_REMAINING_ARRAY_LAYERS
            );

            if (view == VK_NULL_HANDLE) {
                SPDLOG_ERROR("Failed to create image view for texture at binding {} in material '{}'",
                    param.binding, material->name());
                continue;
            }

            // Write image descriptor at appropriate binding
            m_writer.writeImage(
                param.binding,
                view,
                textureParam.sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
            );
        }
    }

    // Update the descriptor set
    m_writer.updateSet(m_renderer.vulkanContext().logical().get(), descriptorSet);

    // Store the descriptor set in the order
    order->materialDescriptorSet = descriptorSet;
}