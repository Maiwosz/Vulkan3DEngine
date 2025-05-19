#pragma once
#include "IAssetHandler.h"
#include "ShaderModule.h"
#include "Handle.h"
#include "LogicalDevice.h"
#include "DescriptorLayoutManager.h"
#include "PipelineLayoutManager.h"
#include "ShaderModuleManager.h"
#include <AssetLib.h>
#include <ShaderLib.h>
#include <unordered_map>
#include <vector>
#include <memory>

struct CombinedShader {
    std::unordered_map<ShaderLib::Stage, ShaderModuleHandle> stages;
};

struct ShaderResources {
    std::unordered_map<uint32_t, DescriptorLayoutHandle> descriptorLayouts; // Set number -> layout handle
    PipelineLayoutHandle pipelineLayout;
};

struct ShaderAsset {
    ShaderHandle handle;
    CombinedShader combinedShader;
    ShaderResources resources;
    ShaderLib::ShaderMetadata metadata;
    uint64_t memorySize;
};

class ShaderManager : public IAssetHandler {
public:
    ShaderManager(
        const LogicalDevice& device,
        ShaderModuleManager& shaderModuleManager,
        DescriptorLayoutManager& descriptorLayoutManager,
        PipelineLayoutManager& pipelineLayoutManager
    );
    ~ShaderManager();

    // IAssetHandler implementation
    bool prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) override;
    void unloadAsset(const std::string& filename) override;
    bool isAssetReady(const std::string& filename) const override;
    uint64_t getAssetSize(const std::string& filename) const override;
    bool isInVram() const override { return true; } // Shaders are stored in GPU memory
    std::vector<AssetDependency> getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const override;
    std::any getResourceInternal(const AssetHandle& handle) const override;
    std::any getHandleInternal(const std::string& filename) const override;

    // Shader-specific accessors (for direct use when you have the handle)
    const ShaderLib::ShaderMetadata& getShaderMetadata(ShaderHandle handle) const;
    const ShaderResources& getShaderResources(ShaderHandle handle) const;
    ShaderModule* getModuleForStage(ShaderHandle shader, ShaderLib::Stage stage);
    const CombinedShader& getCombinedShader(ShaderHandle handle) const;

private:
    // Internal shader management
    ShaderHandle createShader(
        const ShaderLib::ShaderMetadata& metadata,
        const std::vector<ShaderLib::CompiledStage>& stages
    );
    void destroyShader(ShaderHandle handle);

    // Descriptor and pipeline layout creation
    std::unordered_map<uint32_t, DescriptorLayoutHandle> createDescriptorLayouts(
        const ShaderLib::ShaderMetadata& metadata
    );
    PipelineLayoutHandle createPipelineLayout(
        const ShaderLib::ShaderMetadata& metadata,
        const std::unordered_map<uint32_t, DescriptorLayoutHandle>& descriptorLayouts
    );

    // Helper methods
    uint64_t calculateShaderMemorySize(const ShaderAsset& asset) const;
    std::string getShaderCacheKey(const std::string& filename) const;

    // Members
    const LogicalDevice& m_device;
    ShaderModuleManager& m_shaderModuleManager;
    DescriptorLayoutManager& m_descriptorLayoutManager;
    PipelineLayoutManager& m_pipelineLayoutManager;

    // Asset storage
    std::unordered_map<std::string, ShaderAsset> m_shaderAssets; // filename -> shader asset

    // Handle generation
    uint32_t m_nextShaderHandle = 1;
};