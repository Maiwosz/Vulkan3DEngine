#include "MaterialFactory.h"
#include "ImageSamplerUtils.h"
#include "AssetManager.h"
#include "ShaderManager.h"
#include "MaterialSerializer.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

MaterialFactory::MaterialFactory(
    const LogicalDevice& device,
    ShaderManager& shaderManager,
    BufferManager& bufferManager,
    ImageSamplerManager& samplerManager,
    TextureManager& textureManager,
    DescriptorAllocator& descriptorAllocator,
    DescriptorLayoutManager& descriptorLayoutManager,
    ThreadPool& threadPool
)
    : m_device(device),
    m_shaderManager(shaderManager),
    m_bufferManager(bufferManager),
    m_samplerManager(samplerManager),
    m_textureManager(textureManager),
    m_descriptorAllocator(descriptorAllocator),
    m_descriptorLayoutManager(descriptorLayoutManager),
    m_threadPool(threadPool),
    m_asyncMemoryOps(threadPool)
{
}

// =============================================================================
// MATERIAL CREATION - MAIN API
// =============================================================================

std::unique_ptr<Material> MaterialFactory::createMaterial(
    const std::string& name,
    ShaderHandle shaderHandle
) {
    if (!shaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Invalid shader handle");
    }

    // Create default buffers from shader metadata
    auto buffers = createDefaultBuffers(shaderHandle);
    setupAsyncOperations(buffers);

    auto smartShaderHandle = createSmartShaderHandle(shaderHandle);
    if (!smartShaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Failed to create smart shader handle");
    }

    return std::make_unique<Material>(
        name,
        smartShaderHandle,
        buffers,
        m_device,
        m_bufferManager,
        m_samplerManager,
        m_textureManager,
        m_descriptorAllocator,
        m_descriptorLayoutManager
    );
}

std::unique_ptr<Material> MaterialFactory::createMaterial(
    const std::string& name,
    ShaderHandle shaderHandle,
    const std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>& buffers
) {
    if (!shaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Invalid shader handle");
    }

    // Copy provided buffers
    auto materialBuffers = buffers;

    // Fill missing buffers with defaults
    fillMissingBuffers(materialBuffers, shaderHandle);

    setupAsyncOperations(materialBuffers);

    auto smartShaderHandle = createSmartShaderHandle(shaderHandle);
    if (!smartShaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Failed to create smart shader handle");
    }

    return std::make_unique<Material>(
        name,
        smartShaderHandle,
        materialBuffers,
        m_device,
        m_bufferManager,
        m_samplerManager,
        m_textureManager,
        m_descriptorAllocator,
        m_descriptorLayoutManager
    );
}

std::unique_ptr<Material> MaterialFactory::createMaterialFromAsset(
    const std::string& name,
    ShaderHandle shaderHandle,
    const AssetLib::MaterialDefinition& materialDef,
    AssetManager& assetManager
) {
    if (!materialDef.Validate()) {
        throw std::runtime_error("MaterialFactory: Invalid material definition");
    }

    // Create buffers from shader definitions + JSON values
    auto buffers = createBuffersFromAsset(shaderHandle, materialDef.buffers);
    setupAsyncOperations(buffers);

    auto smartShaderHandle = createSmartShaderHandle(shaderHandle);
    if (!smartShaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Failed to create smart shader handle");
    }

    auto material = std::make_unique<Material>(
        name,
        smartShaderHandle,
        buffers,
        m_device,
        m_bufferManager,
        m_samplerManager,
        m_textureManager,
        m_descriptorAllocator,
        m_descriptorLayoutManager
    );

    // Setup textures
    for (const auto& samplerConfig : materialDef.samplers) {
        Material::TextureParam textureParam;
        textureParam.assetHandle = AssetHandle(AssetType::Texture, samplerConfig.texturePath);
        textureParam.colorSpace = samplerConfig.colorSpace;

        try {
            TextureHandle texture = assetManager.getHandle<TextureHandle>(textureParam.assetHandle);
            if (texture.isValid()) {
                textureParam.textureHandle = texture;
            }
        }
        catch (...) {
            // Texture not yet loaded - will be resolved later
        }

        SamplerConfig vkSamplerConfig = ImageSamplerUtils::createSamplerConfig(samplerConfig);
        textureParam.samplerHandle = m_samplerManager.acquireSampler(vkSamplerConfig);

        material->SetTexture(samplerConfig.name, textureParam);
    }

    return material;
}

// =============================================================================
// BUFFER CREATION HELPERS
// =============================================================================

std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>
MaterialFactory::createDefaultBuffers(ShaderHandle shaderHandle) {
    std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>> result;

    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        SPDLOG_DEBUG("MaterialFactory: No custom descriptor set in shader");
        return result;
    }

    // Create instances for all buffer slots in custom descriptor set
    for (const auto& slot : customSet->slots) {
        if (!slot.IsBuffer()) {
            continue;
        }

        // Get buffer definition from custom set
        auto bufferDef = customSet->GetBuffer(slot.name);
        if (!bufferDef) {
            SPDLOG_WARN("MaterialFactory: Buffer definition not found for slot '{}'", slot.name);
            continue;
        }

        // Create instance with default values
        result[slot.name] = bufferDef->CreateInstance();
        SPDLOG_DEBUG("MaterialFactory: Created default buffer '{}' at binding {}",
            slot.name, slot.binding);
    }

    return result;
}

std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>
MaterialFactory::createBuffersFromAsset(
    ShaderHandle shaderHandle,
    const std::unordered_map<std::string, nlohmann::json>& bufferValues
) {
    std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>> result;

    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        SPDLOG_DEBUG("MaterialFactory: No custom descriptor set in shader");
        return result;
    }

    // Create instances for all buffer slots
    for (const auto& slot : customSet->slots) {
        if (!slot.IsBuffer()) {
            continue;
        }

        // Get buffer definition
        auto bufferDef = customSet->GetBuffer(slot.name);
        if (!bufferDef) {
            SPDLOG_WARN("MaterialFactory: Buffer definition not found for slot '{}'", slot.name);
            continue;
        }

        // Check if asset provides values for this buffer
        auto valuesIt = bufferValues.find(slot.name);
        if (valuesIt != bufferValues.end()) {
            // Create instance from shader definition + JSON values
            result[slot.name] = AssetLib::CreateBufferInstanceFromMaterial(
                bufferDef,
                valuesIt->second
            );
            SPDLOG_DEBUG("MaterialFactory: Created buffer '{}' with values from asset", slot.name);
        }
        else {
            // No values in asset - use defaults
            result[slot.name] = bufferDef->CreateInstance();
            SPDLOG_DEBUG("MaterialFactory: Created buffer '{}' with default values", slot.name);
        }
    }

    return result;
}

void MaterialFactory::fillMissingBuffers(
    std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>& buffers,
    ShaderHandle shaderHandle
) {
    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        return;
    }

    // Check each buffer slot in shader
    for (const auto& slot : customSet->slots) {
        if (!slot.IsBuffer()) {
            continue;
        }

        // If buffer not provided, create default
        if (buffers.find(slot.name) == buffers.end()) {
            auto bufferDef = customSet->GetBuffer(slot.name);
            if (bufferDef) {
                buffers[slot.name] = bufferDef->CreateInstance();
                SPDLOG_DEBUG("MaterialFactory: Filled missing buffer '{}' with default", slot.name);
            }
        }
    }
}

std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>
MaterialFactory::cloneBuffers(const Material* sourceMaterial) const {
    std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>> result;

    if (!sourceMaterial) {
        throw std::runtime_error("MaterialFactory: Cannot clone from null material");
    }

    for (const auto& bufferName : sourceMaterial->GetBufferNames()) {
        auto sourceBuffer = sourceMaterial->GetBuffer(bufferName);
        if (sourceBuffer) {
            result[bufferName] = sourceBuffer->Clone();
        }
    }

    return result;
}

// =============================================================================
// HELPER METHODS
// =============================================================================

SmartAssetHandle<ShaderHandle, ShaderAsset> MaterialFactory::createSmartShaderHandle(
    ShaderHandle shaderHandle
) {
    auto* shaderManager = dynamic_cast<ISmartAssetHandler<ShaderHandle, ShaderAsset>*>(&m_shaderManager);
    if (!shaderManager) {
        throw std::runtime_error("MaterialFactory: ShaderManager does not support smart handles");
    }

    return shaderManager->createSmartHandle(shaderHandle);
}

void MaterialFactory::setupAsyncOperations(
    std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>& buffers
) {
    for (auto& [name, buffer] : buffers) {
        if (buffer) {
            buffer->SetAsyncOperations(&m_asyncMemoryOps);
        }
    }
}
