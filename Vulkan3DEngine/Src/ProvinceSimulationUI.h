#pragma once
#include "CppScriptBase.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <string>
#include <chrono>
#include "ProvinceSimulationTest.h"

enum class SortColumn {
    Index,
    Population,
    PopulationGrowth,
    FoodProduction,
    FoodStorage,
    Wealth
};

enum class SortOrder {
    Ascending,
    Descending
};

struct ProvinceDisplayData {
    uint32_t index;
    float population;
    float populationGrowth;
    float foodProduction;
    float foodStorage;
    float wealth;
    const char* status;
};

struct BenchmarkResult {
    SimulationMode mode;
    size_t cpuThreads;
    uint32_t numTicks;
    uint32_t numProvinces;
    double totalTimeMs;
    double avgTimePerTick;
    double ticksPerSecond;
};

using ProvinceStats = ProvinceData;

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
    bool m_showWindow = true;
    bool m_showSettingsWindow = false;
    bool m_showBenchmarkWindow = false;

    std::chrono::high_resolution_clock::time_point m_stepStartTime;
    bool m_stepTimingActive = false;
    double m_lastStepTime = 0;

    // Sorting & filtering
    SortColumn m_sortColumn = SortColumn::Index;
    SortOrder m_sortOrder = SortOrder::Ascending;
    char m_searchBuffer[256] = "";
    int m_selectedProvince = -1;
    bool m_autoScroll = true;

    // Cache for sorted/filtered data
    std::vector<ProvinceDisplayData> m_cachedDisplayData;
    uint32_t m_lastCachedTick = 0xFFFFFFFF;
    SortColumn m_lastSortColumn = SortColumn::Index;
    SortOrder m_lastSortOrder = SortOrder::Ascending;
    std::string m_lastSearchTerm;
    bool m_cacheValid = false;

    // Statistics tracking
    struct TickStats {
        uint32_t tickNumber;
        float totalPopulation;
        float totalWealth;
        float avgGrowth;
        uint32_t growing;
        uint32_t stable;
        uint32_t declining;
        double elapsedMs;
    };
    std::vector<TickStats> m_statsHistory;
    static constexpr size_t MAX_HISTORY = 1000;

    // Settings state
    struct SettingsState {
        SimulationParameters simParams;
        RandomizationParameters randParams;
    };
    SettingsState m_settingsState;
    bool m_settingsChanged = false;

    // Benchmark state
    bool m_benchmarkRunning = false;
    int m_benchmarkTickCount = 100;
    int m_benchmarkCPUThreads = 8;
    bool m_benchmarkGPU = true;
    bool m_benchmarkCPU = true;
    std::vector<BenchmarkResult> m_benchmarkResults;
    std::chrono::high_resolution_clock::time_point m_benchmarkStartTime;
    uint32_t m_benchmarkStartTick = 0;
    SimulationMode m_benchmarkOriginalMode;
    size_t m_benchmarkOriginalThreads = 0;

    enum class BenchmarkPhase {
        Idle,
        GPU_Running,
        GPU_Complete,
        CPU_Running,
        CPU_Complete,
        AllComplete
    };
    BenchmarkPhase m_benchmarkPhase = BenchmarkPhase::Idle;

    void render();
    void renderControlPanel();
    void renderStatsPanel();
    void renderProvinceTable();
    void renderProvinceDetails();
    void renderStatsHistory();
    void renderSettingsWindow();
    void renderBenchmarkWindow();

    const std::vector<ProvinceDisplayData>& getFilteredAndSortedProvinces();
    void rebuildDisplayCache();
    void sortProvinces(std::vector<ProvinceDisplayData>& provinces);
    bool matchesSearch(const ProvinceDisplayData& province, const char* searchTerm);
    void invalidateCache();

    void recordTickStats(uint32_t tickNumber, double elapsedMs);
    const char* getStatusString(float growthPercent);
    ImVec4 getStatusColor(const char* status);

    void loadCurrentSettings();
    void applySettings();
    void resetSettingsToDefault();

    // Benchmark methods
    void startBenchmark();
    void updateBenchmark();
    void completeBenchmarkPhase();
    void finalizeBenchmark();
};
