#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include "ShaderModule.h"
#include "Handle.h"
#include <string>
#include <AssetLib.h>
#include "UniformBufferManager.h"
#include "DescriptorLayoutManager.h"
#include "PipelineLayoutManager.h"

struct CombinedShader {
    std::unordered_map<ShaderLib::Stage, ShaderModuleHandle> stages;
};

struct ShaderResources {
    std::unordered_map<uint32_t, DescriptorLayoutHandle> descriptorLayouts; // Numer seta -> uchwyt layoutu
    PipelineLayoutHandle pipelineLayout;
};

class ShaderModuleManager {
public:
    ShaderModuleManager(
        const LogicalDevice& device,
        DescriptorLayoutManager& descriptorLayoutManager,
        PipelineLayoutManager& pipelineLayoutManager
    );
    ~ShaderModuleManager();

    ShaderModuleHandle createModuleFromSPIRV(const std::vector<uint32_t>& spirvCode);
    ShaderModuleHandle createModuleFromSPIRVFile(const std::string& filePath);
    ShaderHandle createShader(
        const ShaderLib::ShaderMetadata& metadata,
        const std::vector<ShaderLib::CompiledStage>& stages
    );

    void destroyModule(ShaderModuleHandle handle);
    void destroyShader(ShaderHandle handle);

    ShaderModule& getModule(ShaderModuleHandle handle);
    ShaderModule* getModuleForStage(ShaderHandle shader, ShaderLib::Stage stage);
    const CombinedShader& getCombinedShader(ShaderHandle handle) const;
    const ShaderResources& getShaderResources(ShaderHandle handle) const;
    const ShaderLib::ShaderMetadata& getShaderMetadata(ShaderHandle handle) const;

    bool isModuleValid(ShaderModuleHandle handle) const;
    bool isShaderValid(ShaderHandle handle) const;

private:
    std::unordered_map<uint32_t, DescriptorLayoutHandle> createDescriptorLayouts(
        const ShaderLib::ShaderMetadata& metadata
    );

    PipelineLayoutHandle createPipelineLayout(
        const ShaderLib::ShaderMetadata& metadata,
        const std::unordered_map<uint32_t, DescriptorLayoutHandle>& descriptorLayouts
    );

    const LogicalDevice& m_device;
    DescriptorLayoutManager& m_descriptorLayoutManager;
    PipelineLayoutManager& m_pipelineLayoutManager;

    std::unordered_map<ShaderModuleHandle, std::unique_ptr<ShaderModule>> m_modules;

    std::unordered_map<ShaderHandle, CombinedShader> m_combinedShaders;
    std::unordered_map<ShaderHandle, ShaderResources> m_shaderResources;
    std::unordered_map<ShaderHandle, ShaderLib::ShaderMetadata> m_shaderMetadata;

    uint32_t m_nextModuleHandle = 1;
    uint32_t m_nextShaderHandle = 1;
};