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
#include <memory>
#include <string>
#include "ThreadPool.h"
#include "AsyncMemoryOps.h"

class MaterialFactory {
public:
    MaterialFactory(
        const LogicalDevice& device,
        ShaderManager& shaderManager,
        BufferManager& bufferManager,
        ImageSamplerManager& samplerManager,
        TextureManager& textureManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager,
        ThreadPool& threadPool
    );

    // =========================================================================
    //  SIMPLIFIED API - No validation needed
    // =========================================================================

    /**
     * Create material with default buffer instances from shader
     * - Buffers initialized with default values from shader definition
     */
    std::unique_ptr<Material> createMaterial(
        const std::string& name,
        ShaderHandle shaderHandle
    );

    /**
     * Create material with custom buffer instances
     * - Instances must be created from shader definitions (no validation)
     * - Direct pass-through to Material constructor
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
     * - Creates buffer instances from shader definitions
     * - Fills instances with values from materialDef.buffers (JSON)
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
     */
    struct BufferInstanceSet {
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer;
        std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer;
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer;
    };

    BufferInstanceSet cloneBufferInstances(Material* sourceMaterial) const;

private:
    // =========================================================================
    // INTERNAL HELPERS
    // =========================================================================

    /**
     * Create buffer instances from shader definitions + JSON values
     * Simple creation, no validation
     */
    struct BufferSet {
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer;
        std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer;
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer;
    };

    // Create default buffers (all fields have default values)
    BufferSet createDefaultBuffers(ShaderHandle shaderHandle);

    // Create buffers from shader + JSON values from asset
    BufferSet createBuffersFromAsset(
        ShaderHandle shaderHandle,
        const std::unordered_map<std::string, nlohmann::json>& bufferValues
    );

    // Helper: Create smart shader handle
    SmartAssetHandle<ShaderHandle, ShaderAsset> createSmartShaderHandle(ShaderHandle shaderHandle);

    // Helper: Setup async operations for buffer instances
    void setupAsyncOperations(BufferSet& buffers);

    // Dependencies
    const LogicalDevice& m_device;
    ShaderManager& m_shaderManager;
    BufferManager& m_bufferManager;
    ImageSamplerManager& m_samplerManager;
    TextureManager& m_textureManager;
    DescriptorAllocator& m_descriptorAllocator;
    DescriptorLayoutManager& m_descriptorLayoutManager;
    ThreadPool& m_threadPool;

    // Async memory operations
    AsyncMemoryOps m_asyncMemoryOps;
};
