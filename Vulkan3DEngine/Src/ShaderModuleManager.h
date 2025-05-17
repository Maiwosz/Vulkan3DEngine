#pragma once
#include "IAssetHandler.h"
#include "ShaderModule.h"
#include "Handle.h"
#include "LogicalDevice.h"
#include "DescriptorLayoutManager.h"
#include "PipelineLayoutManager.h"
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

class ShaderModuleManager : public IAssetHandler {
public:
    ShaderModuleManager(
        const LogicalDevice& device,
        DescriptorLayoutManager& descriptorLayoutManager,
        PipelineLayoutManager& pipelineLayoutManager
    );
    ~ShaderModuleManager();

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
    ShaderModule* getModule(ShaderModuleHandle handle);

private:
    // Internal shader module management
    ShaderModuleHandle createModuleFromSPIRV(const std::vector<uint32_t>& spirvCode);
    ShaderHandle createShader(
        const ShaderLib::ShaderMetadata& metadata,
        const std::vector<ShaderLib::CompiledStage>& stages
    );
    void destroyModule(ShaderModuleHandle handle);
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
    DescriptorLayoutManager& m_descriptorLayoutManager;
    PipelineLayoutManager& m_pipelineLayoutManager;

    // Asset storage
    std::unordered_map<std::string, ShaderAsset> m_shaderAssets; // filename -> shader asset
    std::unordered_map<ShaderModuleHandle, std::unique_ptr<ShaderModule>> m_modules;

    // Handle generation
    uint32_t m_nextModuleHandle = 1;
    uint32_t m_nextShaderHandle = 1;
};