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
    bool m_editMode = false;

    // Bufory edycji
    char m_editPopulation[32] = "0";
    char m_editFoodProduction[32] = "0";
    char m_editWealth[32] = "0";
    char m_editFoodStorage[32] = "0";

    // Akumulatory dla średnich
    struct PerformanceAccumulators {
        double totalComputeMs = 0.0;
        double totalReadbackMs = 0.0;
        double totalTimeMs = 0.0;
        uint32_t sampleCount = 0;
        uint32_t lastProcessedTick = 0;

        void reset() {
            totalComputeMs = 0.0;
            totalReadbackMs = 0.0;
            totalTimeMs = 0.0;
            sampleCount = 0;
            lastProcessedTick = 0;
        }

        void addSample(double compute, double readback, double total) {
            totalComputeMs += compute;
            totalReadbackMs += readback;
            totalTimeMs += total;
            sampleCount++;
        }
    };
    PerformanceAccumulators m_perfAccumulators;

    // Struktura dla adaptywnej historii populacji
    struct PopulationDataPoint {
        uint32_t tickNumber;
        float population;
    };
    static constexpr size_t MAX_POPULATION_HISTORY = 200;
    std::vector<PopulationDataPoint> m_populationHistory;

    // Historia bogactwa
    struct WealthDataPoint {
        uint32_t tickNumber;
        float wealth;
    };
    static constexpr size_t MAX_WEALTH_HISTORY = 200;
    std::vector<WealthDataPoint> m_wealthHistory;

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

    ImVec4 getStatusColor(float growthPercent);
    const char* getStatusText(float growthPercent);
    void compressPopulationHistory();
    size_t findLeastSignificantPoint() const;
    void compressWealthHistory();
    size_t findLeastSignificantWealthPoint() const;
};
