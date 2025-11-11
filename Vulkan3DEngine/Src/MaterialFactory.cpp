#include "MaterialFactory.h"
#include "ImageSamplerUtils.h"
#include "AssetManager.h"
#include "ShaderManager.h"
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
    m_threadPool(threadPool)
{
}

std::unique_ptr<Material> MaterialFactory::createMaterial(
    const std::string& name,
    ShaderHandle shaderHandle
) {
    if (!shaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Invalid shader handle");
    }

    // Create default buffers from shader - no validation needed
    auto buffers = createDefaultBuffers(shaderHandle);

    auto smartShaderHandle = createSmartShaderHandle(shaderHandle);
    if (!smartShaderHandle.isValid()) {
        throw std::runtime_error("MaterialFactory: Failed to create smart shader handle");
    }

    // Material takes ownership and handles initialization
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
        m_descriptorLayoutManager,
        m_threadPool
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

    // SINGLE validation point - validate and sync all buffers
    auto buffers = prepareBufferInstances(
        shaderHandle,
        inputBuffer,
        outputBuffer,
        inputOutputBuffer
    );

    if (!buffers.isValid) {
        throw std::runtime_error("MaterialFactory: Buffer validation failed");
    }

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
        m_descriptorLayoutManager,
        m_threadPool
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

    // Prepare buffers from asset - single validation point
    auto buffers = prepareBufferInstances(
        shaderHandle,
        materialDef.inputBuffer,
        materialDef.outputBuffer,
        materialDef.inputOutputBuffer
    );

    if (!buffers.isValid) {
        throw std::runtime_error("MaterialFactory: Buffer validation failed");
    }

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
        m_descriptorLayoutManager,
        m_threadPool
    );

    // Setup textures (simple, no validation needed)
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
// PRIVATE: Single validation/sync point
// =============================================================================

MaterialFactory::PreparedBuffers MaterialFactory::prepareBufferInstances(
    ShaderHandle shaderHandle,
    std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer,
    std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer,
    std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer
) {
    PreparedBuffers result;

    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        SPDLOG_DEBUG("MaterialFactory: No custom descriptor set in shader");
        return result; // Valid but empty
    }

    // Validate and sync each buffer - ONCE
    if (inputBuffer) {
        auto inputDef = metadata.GetInputDataBuffer();
        result.inputBuffer = validateAndSyncBuffer(inputDef, inputBuffer, "InputData");
        if (!result.inputBuffer && inputDef) {
            result.isValid = false;
            return result;
        }
    }

    if (outputBuffer) {
        auto outputDef = metadata.GetOutputDataBuffer();
        result.outputBuffer = validateAndSyncBuffer(outputDef, outputBuffer, "OutputData");
        if (!result.outputBuffer && outputDef) {
            result.isValid = false;
            return result;
        }
    }

    if (inputOutputBuffer) {
        auto inputOutputDef = metadata.GetInputOutputDataBuffer();
        result.inputOutputBuffer = validateAndSyncBuffer(inputOutputDef, inputOutputBuffer, "InputOutputData");
        if (!result.inputOutputBuffer && inputOutputDef) {
            result.isValid = false;
            return result;
        }
    }

    return result;
}

std::shared_ptr<ShaderLib::BufferObjectInstance> MaterialFactory::validateAndSyncBuffer(
    std::shared_ptr<const ShaderLib::BufferObjectDefinition> shaderDef,
    std::shared_ptr<const ShaderLib::BufferObjectInstance> instance,
    const std::string& bufferName
) {
    if (!shaderDef) {
        if (instance) {
            SPDLOG_WARN("MaterialFactory: Buffer '{}' provided but shader has no definition", bufferName);
        }
        return nullptr;
    }

    if (!instance) {
        SPDLOG_DEBUG("MaterialFactory: Creating default buffer for '{}'", bufferName);
        return shaderDef->CreateInstance();
    }

    // Single sync operation using simplified API
    ShaderLib::BufferValidator::ValidationReport report;
    auto synchronized = ShaderLib::BufferValidator::ValidateAndSync(
        shaderDef,
        instance,
        &report
    );

    if (!synchronized) {
        SPDLOG_ERROR("MaterialFactory: Failed to sync buffer '{}':", bufferName);
        for (const auto& error : report.errors) {
            SPDLOG_ERROR("  - {}", error);
        }
        return nullptr;
    }

    if (report.HasWarnings()) {
        for (const auto& warning : report.warnings) {
            SPDLOG_WARN("MaterialFactory: Buffer '{}': {}", bufferName, warning);
        }
    }

    // Log what was done
    if (!report.copiedFields.empty()) {
        SPDLOG_DEBUG("MaterialFactory: Buffer '{}' - copied {} fields",
            bufferName, report.copiedFields.size());
    }
    if (!report.defaultFields.empty()) {
        SPDLOG_DEBUG("MaterialFactory: Buffer '{}' - initialized {} fields with defaults",
            bufferName, report.defaultFields.size());
    }

    return synchronized;
}

MaterialFactory::PreparedBuffers MaterialFactory::createDefaultBuffers(ShaderHandle shaderHandle) {
    PreparedBuffers result;

    const auto& metadata = m_shaderManager.getShaderMetadata(shaderHandle);
    const ShaderLib::DescriptorSet* customSet = metadata.GetCustomSet();

    if (!customSet) {
        return result;
    }

    // Create instances - no validation needed (created from shader def)
    if (auto inputDef = metadata.GetInputDataBuffer()) {
        result.inputBuffer = inputDef->CreateInstance();
    }

    if (auto outputDef = metadata.GetOutputDataBuffer()) {
        result.outputBuffer = outputDef->CreateInstance();
    }

    if (auto inputOutputDef = metadata.GetInputOutputDataBuffer()) {
        result.inputOutputBuffer = inputOutputDef->CreateInstance();
    }

    return result;
}

MaterialFactory::BufferInstanceSet MaterialFactory::cloneBufferInstances(const Material* sourceMaterial) const {
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
