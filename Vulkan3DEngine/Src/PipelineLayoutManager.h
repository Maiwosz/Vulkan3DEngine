#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <memory>
#include "PipelineConfig.h"
#include "LogicalDevice.h"
#include "Handle.h"

class PipelineLayoutManager {
public:
    PipelineLayoutManager(const LogicalDevice& device);
    ~PipelineLayoutManager();

    // Tworzy nowy pipeline layout na podstawie konfiguracji
    PipelineLayoutHandle createLayout(const PipelineLayoutConfig& config);

    // Niszczy pipeline layout
    void destroy(PipelineLayoutHandle handle);

    // Pobiera layout
    VkPipelineLayout get(PipelineLayoutHandle handle);

    // Sprawdza, czy handle jest poprawny
    bool isValid(PipelineLayoutHandle handle) const;

    // Aktualizacja stanu (wywołać dla każdej klatki)
    void advanceFrame();

    // Usuwa nieużywane layouty
    void purgeUnusedLayouts(uint64_t ageThresholdFrames);

private:
    // Aktualizuje ostatnie użycie layoutu
    void updateLastUsed(PipelineLayoutHandle handle);

    // Referencja do urządzenia
    const LogicalDevice& m_device;

    // Przechowywanie layoutów
    std::unordered_map<PipelineLayoutHandle, VkPipelineLayout> m_layouts;
    std::unordered_map<PipelineLayoutConfig, PipelineLayoutHandle> m_layoutCache;
    std::unordered_map<PipelineLayoutHandle, uint64_t> m_layoutLastUsed;
    uint32_t m_nextHandle = 1;
    uint64_t m_currentFrame = 0;
};