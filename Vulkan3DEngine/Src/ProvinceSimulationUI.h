#pragma once
#include "CppScriptBase.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <string>
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

    void render();
    void renderControlPanel();
    void renderStatsPanel();
    void renderProvinceTable();
    void renderProvinceDetails();
    void renderStatsHistory();
    void renderSettingsWindow();

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
};
