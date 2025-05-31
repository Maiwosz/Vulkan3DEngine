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

    // Pobiera referencję do pipeline'a
    Pipeline& get(PipelineHandle handle);

    // Sprawdza, czy handle jest poprawny
    bool isValid(PipelineHandle handle) const;

private:
    // Tworzy Vulkan pipeline
    VkPipeline createVkPipeline(const GraphicsPipelineConfig& config, VkPipelineLayout layout);

    // Referencje do potrzebnych komponentów
    const LogicalDevice& m_device;
    ShaderModuleManager& m_shaderManager;
    PipelineLayoutManager& m_layoutManager;

    // Przechowyanie pipeline'ów
    std::unordered_map<PipelineHandle, std::unique_ptr<Pipeline>> m_pipelines;
    std::unordered_map<GraphicsPipelineConfig, PipelineHandle> m_pipelineCache;
    uint32_t m_nextHandle = 1;
};