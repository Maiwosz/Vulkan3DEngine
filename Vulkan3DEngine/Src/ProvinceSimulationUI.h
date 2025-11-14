#pragma once
#include "CppScriptBase.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <vector>
#include "ProvinceSimulationTest.h"

/**
 * Prosty, czytelny interfejs dla symulacji prowincji
 * Skupia się na wydajności i kluczowych statystykach
 */
class ProvinceSimulationUI : public CppScriptBase {
public:
    const char* getScriptName() const override;
    void OnCreate() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;

    void setSimulation(ProvinceSimulationTest* simulation);
    void renderScriptUI() override;

private:
    ProvinceSimulationTest* m_simulation = nullptr;

    // Okna
    bool m_showMainWindow = true;
    bool m_showSettingsWindow = false;
    bool m_showBenchmarkWindow = false;

    // Inspekcja prowincji
    int m_inspectProvinceId = 0;
    char m_inspectBuffer[32] = "0";

    // Wykresy - historia wydajności
    static constexpr size_t PERF_HISTORY_SIZE = 200;
    std::vector<float> m_tickTimeHistory;

    // =========================================================================
    // RENDEROWANIE
    // =========================================================================
    void render();
    void renderMainWindow();
    void renderModeSelector();
    void renderPerformancePanel();
    void renderStatisticsPanel();
    void renderProvinceInspector();
    void renderControlButtons();

    // =========================================================================
    // POMOCNICZE
    // =========================================================================
    void addTickTimeToHistory(float timeMs);
    ImVec4 getStatusColor(float growthPercent);
    const char* getStatusText(float growthPercent);
};
