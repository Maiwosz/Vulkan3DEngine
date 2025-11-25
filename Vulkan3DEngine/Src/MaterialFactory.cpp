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

std::unique_ptr<Material> MaterialFactory::createMaterial(
    const std::string& name,
    ShaderHandle shaderHandle
) {
    if (!shaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Invalid shader handle");
    }

    // Create default buffers - simple, no validation
    auto buffers = createDefaultBuffers(shaderHandle);
    setupAsyncOperations(buffers);

    auto smartShaderHandle = createSmartShaderHandle(shaderHandle);
    if (!smartShaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Failed to create smart shader handle");
    }

    return std::make_unique<Material>(
        name,
        smartShaderHandle,
        buffers.inputBuffer,
        buffers.outputBuffer,
        buffers.inputOutputBuffer,
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
    std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer,
    std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer,
    std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer
) {
    if (!shaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Invalid shader handle");
    }

    // No validation - instances must be created from correct definitions
    BufferSet buffers;
    buffers.inputBuffer = inputBuffer;
    buffers.outputBuffer = outputBuffer;
    buffers.inputOutputBuffer = inputOutputBuffer;

    setupAsyncOperations(buffers);

    auto smartShaderHandle = createSmartShaderHandle(shaderHandle);
    if (!smartShaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Failed to create smart shader handle");
    }

    return std::make_unique<Material>(
        name,
        smartShaderHandle,
        buffers.inputBuffer,
        buffers.outputBuffer,
        buffers.inputOutputBuffer,
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
        buffers.inputBuffer,
        buffers.outputBuffer,
        buffers.inputOutputBuffer,
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
// BUFFER CREATION - Simple, no validation
// =============================================================================

MaterialFactory::BufferSet MaterialFactory::createDefaultBuffers(ShaderHandle shaderHandle) {
    BufferSet result;

    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        SPDLOG_DEBUG("MaterialFactory: No custom descriptor set in shader");
        return result;
    }

    // Create instances with default values
    if (auto inputDef = metadata.GetInputDataBuffer()) {
        result.inputBuffer = inputDef->CreateInstance();
        SPDLOG_DEBUG("MaterialFactory: Created default InputData buffer");
    }

    if (auto outputDef = metadata.GetOutputDataBuffer()) {
        result.outputBuffer = outputDef->CreateInstance();
        SPDLOG_DEBUG("MaterialFactory: Created default OutputData buffer");
    }

    if (auto inputOutputDef = metadata.GetInputOutputDataBuffer()) {
        result.inputOutputBuffer = inputOutputDef->CreateInstance();
        SPDLOG_DEBUG("MaterialFactory: Created default InputOutputData buffer");
    }

    return result;
}

MaterialFactory::BufferSet MaterialFactory::createBuffersFromAsset(
    ShaderHandle shaderHandle,
    const std::unordered_map<std::string, nlohmann::json>& bufferValues
) {
    BufferSet result;

    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        SPDLOG_DEBUG("MaterialFactory: No custom descriptor set in shader");
        return result;
    }

    // Create InputData buffer
    if (auto inputDef = metadata.GetInputDataBuffer()) {
        const std::string bufferName = inputDef->GetName();
        auto valuesIt = bufferValues.find(bufferName);

        if (valuesIt != bufferValues.end()) {
            // Create instance from shader definition + JSON values
            result.inputBuffer = AssetLib::CreateBufferInstanceFromMaterial(
                inputDef,
                valuesIt->second
            );
            SPDLOG_DEBUG("MaterialFactory: Created InputData buffer '{}' with values from asset", bufferName);
        }
        else {
            // No values in asset - use defaults
            result.inputBuffer = inputDef->CreateInstance();
            SPDLOG_DEBUG("MaterialFactory: Created InputData buffer '{}' with default values", bufferName);
        }
    }

    // Create OutputData buffer
    if (auto outputDef = metadata.GetOutputDataBuffer()) {
        const std::string bufferName = outputDef->GetName();
        auto valuesIt = bufferValues.find(bufferName);

        if (valuesIt != bufferValues.end()) {
            result.outputBuffer = AssetLib::CreateBufferInstanceFromMaterial(
                outputDef,
                valuesIt->second
            );
            SPDLOG_DEBUG("MaterialFactory: Created OutputData buffer '{}' with values from asset", bufferName);
        }
        else {
            result.outputBuffer = outputDef->CreateInstance();
            SPDLOG_DEBUG("MaterialFactory: Created OutputData buffer '{}' with default values", bufferName);
        }
    }

    // Create InputOutputData buffer
    if (auto inputOutputDef = metadata.GetInputOutputDataBuffer()) {
        const std::string bufferName = inputOutputDef->GetName();
        auto valuesIt = bufferValues.find(bufferName);

        if (valuesIt != bufferValues.end()) {
            result.inputOutputBuffer = AssetLib::CreateBufferInstanceFromMaterial(
                inputOutputDef,
                valuesIt->second
            );
            SPDLOG_DEBUG("MaterialFactory: Created InputOutputData buffer '{}' with values from asset", bufferName);
        }
        else {
            result.inputOutputBuffer = inputOutputDef->CreateInstance();
            SPDLOG_DEBUG("MaterialFactory: Created InputOutputData buffer '{}' with default values", bufferName);
        }
    }

    return result;
}

MaterialFactory::BufferInstanceSet MaterialFactory::cloneBufferInstances(Material* sourceMaterial) const {
    if (!sourceMaterial) {
        throw std::runtime_error("MaterialFactory: Cannot clone from null material");
    }

    BufferInstanceSet result;

    if (sourceMaterial->HasInputBuffer()) {
        result.inputBuffer = sourceMaterial->GetInputBuffer()->Clone();
    }

    if (sourceMaterial->HasOutputBuffer()) {
        result.outputBuffer = sourceMaterial->GetOutputBuffer()->Clone();
    }

    if (sourceMaterial->HasInputOutputBuffer()) {
        result.inputOutputBuffer = sourceMaterial->GetInputOutputBuffer()->Clone();
    }

    return result;
}

SmartAssetHandle<ShaderHandle, ShaderAsset> MaterialFactory::createSmartShaderHandle(ShaderHandle shaderHandle) {
    auto* shaderManager = dynamic_cast<ISmartAssetHandler<ShaderHandle, ShaderAsset>*>(&m_shaderManager);
    if (!shaderManager) {
        throw std::runtime_error("MaterialFactory: ShaderManager does not support smart handles");
    }

    return shaderManager->createSmartHandle(shaderHandle);
}

void MaterialFactory::setupAsyncOperations(BufferSet& buffers) {
    if (buffers.inputBuffer) {
        buffers.inputBuffer->SetAsyncOperations(&m_asyncMemoryOps);
    }
    if (buffers.outputBuffer) {
        buffers.outputBuffer->SetAsyncOperations(&m_asyncMemoryOps);
    }
    if (buffers.inputOutputBuffer) {
        buffers.inputOutputBuffer->SetAsyncOperations(&m_asyncMemoryOps);
    }
}
