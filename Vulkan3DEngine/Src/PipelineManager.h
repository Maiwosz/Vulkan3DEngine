#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <memory>
#include "Pipeline.h"
#include "PipelineConfig.h"
#include "ShaderModuleManager.h"
#include "PipelineLayoutManager.h"
#include "LogicalDevice.h"
#include "Handle.h"


class PipelineManager {
public:
    PipelineManager(const LogicalDevice& device, ShaderModuleManager& shaderManager, PipelineLayoutManager& layoutManager);
    ~PipelineManager();

    // Tworzy nowy pipeline graficzny na podstawie konfiguracji
    PipelineHandle createGraphicsPipeline(const GraphicsPipelineConfig& config);

    // Tworzy nowy pipeline obliczeniowy na podstawie konfiguracji
    PipelineHandle createComputePipeline(const ComputePipelineConfig& config);

    // Pobiera referencję do pipeline'a
    Pipeline& get(PipelineHandle handle);

    // Sprawdza, czy handle jest poprawny
    bool isValid(PipelineHandle handle) const;

private:
    // Tworzy Vulkan graphics pipeline
    VkPipeline createVkGraphicsPipeline(const GraphicsPipelineConfig& config, VkPipelineLayout layout);

    // Tworzy Vulkan compute pipeline
    VkPipeline createVkComputePipeline(const ComputePipelineConfig& config, VkPipelineLayout layout);

    // Waliduje konfigurację graphics pipeline
    bool validateGraphicsPipelineConfig(const GraphicsPipelineConfig& config) const;

    // Waliduje konfigurację compute pipeline
    bool validateComputePipelineConfig(const ComputePipelineConfig& config) const;

    // Referencje do potrzebnych komponentów
    const LogicalDevice& m_device;
    ShaderModuleManager& m_shaderManager;
    PipelineLayoutManager& m_layoutManager;

    // Przechowywanie pipeline'ów
    std::unordered_map<PipelineHandle, std::unique_ptr<Pipeline>> m_pipelines;
    std::unordered_map<GraphicsPipelineConfig, PipelineHandle> m_graphicsPipelineCache;
    std::unordered_map<ComputePipelineConfig, PipelineHandle> m_computePipelineCache;
    uint32_t m_nextHandle = 1;
};