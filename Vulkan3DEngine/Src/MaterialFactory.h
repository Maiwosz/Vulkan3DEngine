#pragma once
#include "Material.h"
#include "ShaderManager.h"
#include "BufferManager.h"
#include "ImageSamplerManager.h"
#include "TextureManager.h"
#include "DescriptorAllocator.h"
#include "DescriptorLayoutManager.h"
#include "AssetLib.h"
#include "BufferObjectDefinition.h"
#include "BufferObjectInstance.h"
#include "BufferValidator.h"
#include <memory>
#include <string>

class MaterialFactory {
public:
    MaterialFactory(
        const LogicalDevice& device,
        ShaderManager& shaderManager,
        BufferManager& bufferManager,
        ImageSamplerManager& samplerManager,
        TextureManager& textureManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager
    );

    // =========================================================================
    //  API - Three clear creation paths
    // =========================================================================

    /**
     * Create material with default buffer instances from shader
     * - Buffers initialized with default values
     * - No validation needed (created from shader definition)
     */
    std::unique_ptr<Material> createMaterial(
        const std::string& name,
        ShaderHandle shaderHandle
    );

    /**
     * Create material with custom buffer instances
     * - Validates and synchronizes buffers against shader
     * - Single validation point
     */
    std::unique_ptr<Material> createMaterial(
        const std::string& name,
        ShaderHandle shaderHandle,
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer,
        std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer,
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer
    );

    /**
     * Create material from asset definition
     * - Validates asset buffers against shader
     * - Initializes textures
     */
    std::unique_ptr<Material> createMaterialFromAsset(
        const std::string& name,
        ShaderHandle shaderHandle,
        const AssetLib::MaterialDefinition& materialDef,
        AssetManager& assetManager
    );

    /**
     * Clone buffer instances from material (for creating variants)
     * Simple wrapper - no validation needed
     */
    struct BufferInstanceSet {
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer;
        std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer;
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer;
    };

    BufferInstanceSet cloneBufferInstances(const Material* sourceMaterial) const;

private:
    // =========================================================================
    // SINGLE VALIDATION/SYNC POINT - Called once during creation
    // =========================================================================

    /**
     * Prepare buffer instances for material
     * - Validates against shader definition
     * - Synchronizes structure if needed
     * - Returns validated instances ready for use
     *
     * This is the ONLY place where validation happens!
     */
    struct PreparedBuffers {
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer;
        std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer;
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer;
        bool isValid = true;
    };

    PreparedBuffers prepareBufferInstances(
        ShaderHandle shaderHandle,
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer,
        std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer,
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer
    );

    // Helper: Validate and sync single buffer
    std::shared_ptr<ShaderLib::BufferObjectInstance> validateAndSyncBuffer(
        std::shared_ptr<const ShaderLib::BufferObjectDefinition> shaderDef,
        std::shared_ptr<const ShaderLib::BufferObjectInstance> instance,
        const std::string& bufferName
    );

    // Helper: Create default buffers from shader
    PreparedBuffers createDefaultBuffers(ShaderHandle shaderHandle);

    // Helper: Create smart shader handle
    SmartAssetHandle<ShaderHandle, ShaderAsset> createSmartShaderHandle(ShaderHandle shaderHandle);

    // Dependencies
    const LogicalDevice& m_device;
    ShaderManager& m_shaderManager;
    BufferManager& m_bufferManager;
    ImageSamplerManager& m_samplerManager;
    TextureManager& m_textureManager;
    DescriptorAllocator& m_descriptorAllocator;
    DescriptorLayoutManager& m_descriptorLayoutManager;
};
