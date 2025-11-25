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
#include <unordered_map>
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
    //  MATERIAL CREATION API
    // =========================================================================

    /**
     * Create material with default buffer instances from shader
     * - Buffers initialized with default values from shader definition
     * - Creates instances for all buffers defined in custom descriptor set
     */
    std::unique_ptr<Material> createMaterial(
        const std::string& name,
        ShaderHandle shaderHandle
    );

    /**
     * Create material with custom buffer instances
     * - Buffers identified by name (from shader metadata)
     * - Missing buffers will be created with defaults
     */
    std::unique_ptr<Material> createMaterial(
        const std::string& name,
        ShaderHandle shaderHandle,
        const std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>& buffers
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
     * Clone all buffers from material (for creating variants)
     */
    std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>
        cloneBuffers(const Material* sourceMaterial) const;

private:
    // =========================================================================
    // BUFFER CREATION HELPERS
    // =========================================================================

    // Create default buffers from shader metadata
    std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>
        createDefaultBuffers(ShaderHandle shaderHandle);

    // Create buffers from shader + JSON values from asset
    std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>
        createBuffersFromAsset(
            ShaderHandle shaderHandle,
            const std::unordered_map<std::string, nlohmann::json>& bufferValues
        );

    // Fill missing buffers with defaults
    void fillMissingBuffers(
        std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>& buffers,
        ShaderHandle shaderHandle
    );

    // Helper: Create smart shader handle
    SmartAssetHandle<ShaderHandle, ShaderAsset> createSmartShaderHandle(ShaderHandle shaderHandle);

    // Helper: Setup async operations for buffer instances
    void setupAsyncOperations(
        std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>& buffers
    );

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
