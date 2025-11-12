#include "ProvinceSimulationUI.h"
#include "ProvinceSimulationTest.h"
#include "IImGuiProvider.h"
#include <algorithm>
#include <chrono>

const char* ProvinceSimulationUI::getScriptName() const {
    return "ProvinceSimulationUI";
}

void ProvinceSimulationUI::OnCreate() {
    SPDLOG_INFO("ProvinceSimulationUI created for entity {}", entity.id);

    auto* engine = getEngine();
    if (engine) {
        auto imguiProvider = engine->engineCore().renderer().getImGuiProvider();
        if (imguiProvider) {
            imguiProvider->registerCallback([this]() {
                render();
                });
            SPDLOG_INFO("ImGui callback registered for ProvinceSimulationUI");
        }
        else {
            SPDLOG_ERROR("ImGui provider not available");
        }
    }
}

void ProvinceSimulationUI::OnUpdate(float deltaTime) {
    if (!m_simulation) return;

    // Handle benchmark updates
    if (m_benchmarkRunning) {
        updateBenchmark();
    }

    static uint32_t lastCompletedTick = 0;
    uint32_t currentTick = m_simulation->getCurrentTick();

    if (m_stepTimingActive && currentTick != lastCompletedTick) {
        if (!m_simulation->isComputeInProgress()) {
            auto end = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(end - m_stepStartTime).count();
            m_lastStepTime = elapsed;

            recordTickStats(currentTick, elapsed);

            m_stepTimingActive = false;
            lastCompletedTick = currentTick;
            invalidateCache();
        }
    }
}

void ProvinceSimulationUI::OnDestroy() {
    SPDLOG_INFO("ProvinceSimulationUI destroyed for entity {}", entity.id);
    m_simulation = nullptr;
}

void ProvinceSimulationUI::setSimulation(ProvinceSimulationTest* simulation) {
    m_simulation = simulation;
    invalidateCache();
    loadCurrentSettings();
    SPDLOG_INFO("Simulation reference set in UI");
}

void ProvinceSimulationUI::renderScriptUI() {
    ImGui::Text("Province Simulation UI Controller");
    ImGui::Text("Simulation: %s", m_simulation ? "Connected" : "Not Connected");
    ImGui::Checkbox("Show Window", &m_showWindow);
    ImGui::Checkbox("Show Settings", &m_showSettingsWindow);
}

void ProvinceSimulationUI::invalidateCache() {
    m_cacheValid = false;
}

void ProvinceSimulationUI::render() {
    if (!m_simulation) return;

    if (m_showWindow) {
        ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Province Simulation Control", &m_showWindow)) {
            renderControlPanel();
            ImGui::Separator();

            ImGui::BeginChild("MainContent", ImVec2(0, -200), false);
            {
                ImGui::BeginChild("ProvinceList", ImVec2(ImGui::GetContentRegionAvail().x * 0.7f, 0), true);
                renderProvinceTable();
                ImGui::EndChild();

                ImGui::SameLine();

                ImGui::BeginChild("ProvinceDetails", ImVec2(0, 0), true);
                renderProvinceDetails();
                ImGui::EndChild();
            }
            ImGui::EndChild();

            ImGui::Separator();

            ImGui::BeginChild("StatsSection", ImVec2(0, 0), false);
            {
                ImGui::BeginChild("CurrentStats", ImVec2(ImGui::GetContentRegionAvail().x * 0.4f, 0), true);
                renderStatsPanel();
                ImGui::EndChild();

                ImGui::SameLine();

                ImGui::BeginChild("StatsHistory", ImVec2(0, 0), true);
                renderStatsHistory();
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    if (m_showSettingsWindow) {
        renderSettingsWindow();
    }

    if (m_showBenchmarkWindow) {
        renderBenchmarkWindow();
    }
}


void ProvinceSimulationUI::renderControlPanel() {
    ImGui::Text("Simulation Control");
    ImGui::SameLine(ImGui::GetWindowWidth() - 350);

    // Mode selector
    SimulationMode currentMode = m_simulation->getMode();
    const char* modeNames[] = { "GPU", "CPU" };
    int modeIdx = (int)currentMode;
    ImGui::SetNextItemWidth(80);
    if (ImGui::Combo("Mode", &modeIdx, modeNames, 2)) {
        m_simulation->setMode((SimulationMode)modeIdx);
        invalidateCache();
    }

    ImGui::SameLine();

    // CPU thread count (only for CPU mode)
    if (currentMode == SimulationMode::CPU) {
        int threads = (int)m_simulation->getCPUThreadCount();
        ImGui::SetNextItemWidth(80);
        if (ImGui::InputInt("Threads", &threads, 1, 4)) {
            if (threads < 1) threads = 1;
            if (threads > 64) threads = 64;
            m_simulation->setCPUThreadCount(threads);
        }
        ImGui::SameLine();
    }

    if (ImGui::Button("Settings", ImVec2(80, 0))) {
        m_showSettingsWindow = !m_showSettingsWindow;
        if (m_showSettingsWindow) {
            loadCurrentSettings();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Benchmark", ImVec2(90, 0))) {
        m_showBenchmarkWindow = !m_showBenchmarkWindow;
    }

    ImGui::SameLine();
    uint32_t currentTick = m_simulation->getCurrentTick();
    ImGui::Text("Tick: %u", currentTick);

    bool computeInProgress = m_simulation->isComputeInProgress();
    if (computeInProgress) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "[COMPUTING...]");
    }

    ImGui::Spacing();

    if (computeInProgress) ImGui::BeginDisabled();

    if (ImGui::Button("Single Step", ImVec2(120, 0))) {
        m_stepStartTime = std::chrono::high_resolution_clock::now();
        m_stepTimingActive = true;
        m_simulation->runSingleStep();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Run one simulation tick (async, non-blocking)");
    }

    ImGui::SameLine();

    static int ticksToRun = 10;
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("##TickCount", &ticksToRun, 1, 100);
    if (ticksToRun < 1) ticksToRun = 1;
    if (ticksToRun > 10000) ticksToRun = 10000;

    ImGui::SameLine();
    if (ImGui::Button("Run N Ticks", ImVec2(120, 0))) {
        m_stepStartTime = std::chrono::high_resolution_clock::now();
        m_stepTimingActive = true;
        m_simulation->runMultipleSteps(ticksToRun);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Run multiple ticks (async, queued execution)");
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset Simulation", ImVec2(120, 0))) {
        m_simulation->resetSimulation();
        m_statsHistory.clear();
        m_selectedProvince = -1;
        m_lastStepTime = 0;
        invalidateCache();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Reset simulation to initial state (blocking)");
    }

    if (computeInProgress) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Refresh Data", ImVec2(120, 0))) {
        m_simulation->requestDataRefresh();
        invalidateCache();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Manually refresh cached data from GPU (async)");
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);

    if (m_lastStepTime > 0) {
        ImGui::Text("Last Step Time: %.3f ms", m_lastStepTime);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Search and sort controls
    ImGui::Text("Filter & Sort:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(250);
    if (ImGui::InputText("##Search", m_searchBuffer, sizeof(m_searchBuffer))) {
        invalidateCache(); // Invalidate when search changes
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Search by province index");
    }

    ImGui::SameLine();
    ImGui::Text("Sort by:");
    ImGui::SameLine();

    const char* sortOptions[] = { "Index", "Population", "Growth %", "Food Prod", "Food Storage", "Wealth" };
    int currentSort = (int)m_sortColumn;
    ImGui::SetNextItemWidth(120);
    if (ImGui::Combo("##SortColumn", &currentSort, sortOptions, IM_ARRAYSIZE(sortOptions))) {
        m_sortColumn = (SortColumn)currentSort;
        invalidateCache(); // Invalidate when sort changes
    }

    ImGui::SameLine();
    if (ImGui::Button(m_sortOrder == SortOrder::Ascending ? "↑ Asc" : "↓ Desc")) {
        m_sortOrder = (m_sortOrder == SortOrder::Ascending) ? SortOrder::Descending : SortOrder::Ascending;
        invalidateCache(); // Invalidate when order changes
    }
}

void ProvinceSimulationUI::renderStatsPanel() {
    ImGui::Text("Current Statistics");
    ImGui::Separator();

    if (m_statsHistory.empty()) {
        ImGui::TextDisabled("No simulation data yet");
        return;
    }

    const auto& latest = m_statsHistory.back();

    ImGui::Text("Tick: %u", latest.tickNumber);
    ImGui::Text("Last Tick Time: %.3f ms", latest.elapsedMs);
    ImGui::Spacing();

    ImGui::Text("Total Population: %.1f k", latest.totalPopulation);
    ImGui::Text("Total Wealth: %.1f", latest.totalWealth);
    ImGui::Text("Average Growth: %.2f%%", latest.avgGrowth);
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Growing: %u", latest.growing);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "Stable: %u", latest.stable);
    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Declining: %u", latest.declining);

    ImGui::Spacing();
    ImGui::Separator();

    if (m_statsHistory.size() >= 2) {
        const auto& prev = m_statsHistory[m_statsHistory.size() - 2];
        float popChange = latest.totalPopulation - prev.totalPopulation;
        float wealthChange = latest.totalWealth - prev.totalWealth;

        ImGui::Text("Population Change: %+.1f k", popChange);
        ImGui::Text("Wealth Change: %+.1f", wealthChange);
    }
}

void ProvinceSimulationUI::renderProvinceTable() {
    ImGui::Text("Provinces (%u total)", m_simulation->getNumProvinces());
    ImGui::Separator();

    // Get cached data (only rebuilds if needed)
    const auto& provinces = getFilteredAndSortedProvinces();

    ImGui::Text("Showing %zu provinces", provinces.size());
    ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable;

    if (ImGui::BeginTable("ProvinceTable", 7, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Population", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Growth %", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Food Prod", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Food Storage", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Wealth", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(provinces.size());
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                const auto& prov = provinces[row];

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable(std::to_string(prov.index).c_str(),
                    m_selectedProvince == (int)prov.index,
                    ImGuiSelectableFlags_SpanAllColumns)) {
                    m_selectedProvince = prov.index;
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f k", prov.population);

                ImGui::TableSetColumnIndex(2);
                ImVec4 growthColor = prov.populationGrowth > 0 ?
                    ImVec4(0.2f, 1.0f, 0.2f, 1.0f) :
                    ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
                ImGui::TextColored(growthColor, "%+.1f%%", prov.populationGrowth);

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.2f", prov.foodProduction);

                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.1f", prov.foodStorage);

                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%.1f", prov.wealth);

                ImGui::TableSetColumnIndex(6);
                ImGui::TextColored(getStatusColor(prov.status), "%s", prov.status);
            }
        }
        clipper.End();

        ImGui::EndTable();
    }
}

void ProvinceSimulationUI::renderProvinceDetails() {
    ImGui::Text("Province Details");
    ImGui::Separator();

    if (m_selectedProvince < 0) {
        ImGui::TextDisabled("Select a province from the table");
        return;
    }

    auto data = m_simulation->getProvinceData(m_selectedProvince);
    auto initial = m_simulation->getInitialStats(m_selectedProvince);

    ImGui::Text("Province ID: %d", m_selectedProvince);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Current State:");
    ImGui::Text("Population: %.1f k", data.population);
    ImGui::Text("Food Production: %.2f /tick", data.foodProductionModifier);
    ImGui::Text("Food Storage: %.1f", data.foodStorage);
    ImGui::Text("Wealth: %.1f", data.wealth);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Initial State:");
    ImGui::Text("Population: %.1f k", initial.population);
    ImGui::Text("Food Production: %.2f /tick", initial.foodProductionModifier);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    float popChange = data.population - initial.population;
    float popChangePercent = (popChange / initial.population) * 100.0f;

    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Changes:");
    ImGui::Text("Population: %+.1f k (%+.1f%%)", popChange, popChangePercent);
    ImGui::Text("Wealth Accumulated: %.1f", data.wealth);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const char* status = getStatusString(popChangePercent);
    ImGui::Text("Status: ");
    ImGui::SameLine();
    ImGui::TextColored(getStatusColor(status), "%s", status);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Food Balance:");

    float foodConsumption = data.population * m_simulation->getFoodConsumptionPerPop();
    float foodBalance = data.foodProductionModifier - foodConsumption;

    ImGui::Text("Production: %.2f", data.foodProductionModifier);
    ImGui::Text("Consumption: %.2f", foodConsumption);
    ImGui::Text("Balance: %+.2f", foodBalance);

    if (foodBalance > 0) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Surplus!");
    }
    else {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Deficit!");
    }
}

void ProvinceSimulationUI::renderStatsHistory() {
    ImGui::Text("Statistics History");
    ImGui::Separator();

    if (m_statsHistory.empty()) {
        ImGui::TextDisabled("No history yet");
        return;
    }

    if (m_statsHistory.size() > 1) {
        std::vector<float> popValues;
        popValues.reserve(m_statsHistory.size());
        for (const auto& stat : m_statsHistory) {
            popValues.push_back(stat.totalPopulation);
        }

        ImGui::PlotLines("Total Population", popValues.data(), popValues.size(),
            0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 80));

        std::vector<float> wealthValues;
        wealthValues.reserve(m_statsHistory.size());
        for (const auto& stat : m_statsHistory) {
            wealthValues.push_back(stat.totalWealth);
        }

        ImGui::PlotLines("Total Wealth", wealthValues.data(), wealthValues.size(),
            0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 80));
    }

    ImGui::Separator();
    ImGui::Text("History: %zu entries", m_statsHistory.size());
    if (ImGui::Button("Clear History")) {
        m_statsHistory.clear();
    }
}

void ProvinceSimulationUI::renderSettingsWindow() {
    ImGui::SetNextWindowSize(ImVec2(600, 700), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Simulation Settings", &m_showSettingsWindow)) {
        bool canEdit = !m_simulation->isComputeInProgress();

        if (!canEdit) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f),
                "Settings locked during computation");
            ImGui::Separator();
        }

        if (!canEdit) ImGui::BeginDisabled();

        // Simulation Parameters Section
        if (ImGui::CollapsingHeader("Simulation Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();

            int numProvinces = m_settingsState.simParams.numProvinces;
            if (ImGui::InputInt("Number of Provinces", &numProvinces, 1024, 8192)) {
                if (numProvinces < 1024) numProvinces = 1024;
                if (numProvinces > 1048576) numProvinces = 1048576;
                m_settingsState.simParams.numProvinces = numProvinces;
                m_settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Range: 1024 - 1048576");
            }

            if (ImGui::DragFloat("Food Consumption per Pop",
                &m_settingsState.simParams.foodConsumptionPerPop,
                0.01f, 0.01f, 1.0f, "%.3f")) {
                m_settingsChanged = true;
            }

            if (ImGui::DragFloat("Base Population Growth",
                &m_settingsState.simParams.basePopulationGrowth,
                0.001f, 0.0f, 0.1f, "%.4f")) {
                m_settingsChanged = true;
            }

            if (ImGui::DragFloat("Starvation Threshold",
                &m_settingsState.simParams.starvationThreshold,
                0.05f, 0.0f, 1.0f, "%.2f")) {
                m_settingsChanged = true;
            }

            if (ImGui::DragFloat("Wealth per Pop",
                &m_settingsState.simParams.wealthPerPop,
                0.05f, 0.0f, 10.0f, "%.2f")) {
                m_settingsChanged = true;
            }

            if (ImGui::DragFloat("Max Food Storage",
                &m_settingsState.simParams.maxFoodStorage,
                5.0f, 10.0f, 1000.0f, "%.1f")) {
                m_settingsChanged = true;
            }

            if (ImGui::DragFloat("Min Population",
                &m_settingsState.simParams.minPopulation,
                0.01f, 0.01f, 1.0f, "%.2f")) {
                m_settingsChanged = true;
            }

            ImGui::Unindent();
        }

        ImGui::Spacing();

        // Randomization Parameters Section
        if (ImGui::CollapsingHeader("Randomization Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();

            if (ImGui::DragFloat("Min Initial Population",
                &m_settingsState.randParams.minPopulation,
                0.5f, 0.1f, 100.0f, "%.1f")) {
                if (m_settingsState.randParams.minPopulation > m_settingsState.randParams.maxPopulation) {
                    m_settingsState.randParams.maxPopulation = m_settingsState.randParams.minPopulation;
                }
                m_settingsChanged = true;
            }

            if (ImGui::DragFloat("Max Initial Population",
                &m_settingsState.randParams.maxPopulation,
                0.5f, 0.1f, 100.0f, "%.1f")) {
                if (m_settingsState.randParams.maxPopulation < m_settingsState.randParams.minPopulation) {
                    m_settingsState.randParams.minPopulation = m_settingsState.randParams.maxPopulation;
                }
                m_settingsChanged = true;
            }

            ImGui::Spacing();

            if (ImGui::DragFloat("Min Food Production",
                &m_settingsState.randParams.minFoodProduction,
                0.1f, 0.1f, 50.0f, "%.2f")) {
                if (m_settingsState.randParams.minFoodProduction > m_settingsState.randParams.maxFoodProduction) {
                    m_settingsState.randParams.maxFoodProduction = m_settingsState.randParams.minFoodProduction;
                }
                m_settingsChanged = true;
            }

            if (ImGui::DragFloat("Max Food Production",
                &m_settingsState.randParams.maxFoodProduction,
                0.1f, 0.1f, 50.0f, "%.2f")) {
                if (m_settingsState.randParams.maxFoodProduction < m_settingsState.randParams.minFoodProduction) {
                    m_settingsState.randParams.minFoodProduction = m_settingsState.randParams.maxFoodProduction;
                }
                m_settingsChanged = true;
            }

            ImGui::Spacing();

            if (ImGui::DragFloat("Initial Food Storage",
                &m_settingsState.randParams.initialFoodStorage,
                1.0f, 0.0f, 100.0f, "%.1f")) {
                m_settingsChanged = true;
            }

            ImGui::Spacing();

            int seed = m_settingsState.randParams.randomSeed;
            if (ImGui::InputInt("Random Seed (0 = random)", &seed, 1, 100)) {
                if (seed < 0) seed = 0;
                m_settingsState.randParams.randomSeed = seed;
                m_settingsChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Set to 0 for non-deterministic random generation");
            }

            ImGui::Unindent();
        }

        if (!canEdit) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Action buttons
        if (!canEdit) ImGui::BeginDisabled();

        if (ImGui::Button("Apply Settings", ImVec2(150, 0))) {
            applySettings();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Apply settings without resetting simulation");
        }

        ImGui::SameLine();

        if (ImGui::Button("Apply & Reset", ImVec2(150, 0))) {
            m_simulation->resetSimulationWithParameters(
                m_settingsState.simParams,
                m_settingsState.randParams
            );
            m_statsHistory.clear();
            m_selectedProvince = -1;
            m_lastStepTime = 0;
            invalidateCache();
            m_settingsChanged = false;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Apply settings and reset simulation to initial state");
        }

        ImGui::SameLine();

        if (ImGui::Button("Reset to Defaults", ImVec2(150, 0))) {
            resetSettingsToDefault();
        }

        if (!canEdit) ImGui::EndDisabled();

        ImGui::SameLine();

        if (ImGui::Button("Reload Current", ImVec2(150, 0))) {
            loadCurrentSettings();
            m_settingsChanged = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reload settings from simulation (discard changes)");
        }

        if (m_settingsChanged) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f),
                "* Settings have been modified");
        }
    }
    ImGui::End();
}

void ProvinceSimulationUI::renderBenchmarkWindow() {
    ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Performance Benchmark", &m_showBenchmarkWindow)) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "GPU vs CPU Performance Comparison");
        ImGui::Separator();
        ImGui::Spacing();

        bool canStart = !m_benchmarkRunning && !m_simulation->isComputeInProgress();

        if (!canStart) ImGui::BeginDisabled();

        ImGui::Text("Benchmark Configuration:");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(200);
        ImGui::InputInt("Number of Ticks", &m_benchmarkTickCount, 10, 100);
        if (m_benchmarkTickCount < 10) m_benchmarkTickCount = 10;
        if (m_benchmarkTickCount > 10000) m_benchmarkTickCount = 10000;

        ImGui::SetNextItemWidth(200);
        ImGui::InputInt("CPU Threads", &m_benchmarkCPUThreads, 1, 4);
        if (m_benchmarkCPUThreads < 1) m_benchmarkCPUThreads = 1;
        if (m_benchmarkCPUThreads > 64) m_benchmarkCPUThreads = 64;

        ImGui::Spacing();
        ImGui::Checkbox("Benchmark GPU", &m_benchmarkGPU);
        ImGui::Checkbox("Benchmark CPU", &m_benchmarkCPU);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Run Benchmark", ImVec2(200, 40))) {
            startBenchmark();
        }

        if (!canStart) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Show benchmark progress
        if (m_benchmarkRunning) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "Benchmark running...");

            const char* phaseText = "";
            switch (m_benchmarkPhase) {
            case BenchmarkPhase::GPU_Running:
                phaseText = "Testing GPU performance...";
                break;
            case BenchmarkPhase::CPU_Running:
                phaseText = "Testing CPU performance...";
                break;
            default:
                phaseText = "Preparing...";
            }
            ImGui::Text("%s", phaseText);

            uint32_t currentTick = m_simulation->getCurrentTick();
            uint32_t ticksCompleted = currentTick - m_benchmarkStartTick;
            float progress = (float)ticksCompleted / m_benchmarkTickCount;
            ImGui::ProgressBar(progress, ImVec2(-1, 0));
            ImGui::Text("Tick: %u / %u", ticksCompleted, m_benchmarkTickCount);
        }

        // Show results
        if (!m_benchmarkResults.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Benchmark Results:");
            ImGui::Separator();

            ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;

            if (ImGui::BeginTable("BenchmarkResults", 6, flags)) {
                ImGui::TableSetupColumn("Mode");
                ImGui::TableSetupColumn("Threads");
                ImGui::TableSetupColumn("Ticks");
                ImGui::TableSetupColumn("Total Time (ms)");
                ImGui::TableSetupColumn("Avg/Tick (ms)");
                ImGui::TableSetupColumn("Ticks/sec");
                ImGui::TableHeadersRow();

                for (const auto& result : m_benchmarkResults) {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", result.mode == SimulationMode::GPU ? "GPU" : "CPU");

                    ImGui::TableSetColumnIndex(1);
                    if (result.mode == SimulationMode::CPU) {
                        ImGui::Text("%zu", result.cpuThreads);
                    }
                    else {
                        ImGui::Text("-");
                    }

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%u", result.numTicks);

                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%.2f", result.totalTimeMs);

                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%.3f", result.avgTimePerTick);

                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%.1f", result.ticksPerSecond);
                }

                ImGui::EndTable();
            }

            // Performance comparison
            if (m_benchmarkResults.size() >= 2) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Text("Performance Comparison:");

                auto& gpuResult = m_benchmarkResults[0];
                auto& cpuResult = m_benchmarkResults[1];

                if (gpuResult.mode == SimulationMode::GPU && cpuResult.mode == SimulationMode::CPU) {
                    double speedup = cpuResult.totalTimeMs / gpuResult.totalTimeMs;

                    ImGui::Text("GPU vs CPU Speedup: %.2fx", speedup);

                    if (speedup > 1.0) {
                        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
                            "GPU is %.1f%% faster", (speedup - 1.0) * 100.0);
                    }
                    else {
                        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                            "CPU is %.1f%% faster", (1.0 / speedup - 1.0) * 100.0);
                    }
                }
            }

            ImGui::Spacing();
            if (ImGui::Button("Clear Results")) {
                m_benchmarkResults.clear();
            }
        }
    }
    ImGui::End();
}

const std::vector<ProvinceDisplayData>& ProvinceSimulationUI::getFilteredAndSortedProvinces() {
    uint32_t currentTick = m_simulation->getCurrentTick();
    std::string currentSearch(m_searchBuffer);

    // Check if cache is still valid
    bool needsRebuild = !m_cacheValid ||
        m_lastCachedTick != currentTick ||
        m_lastSortColumn != m_sortColumn ||
        m_lastSortOrder != m_sortOrder ||
        m_lastSearchTerm != currentSearch;

    if (needsRebuild) {
        rebuildDisplayCache();
        m_lastCachedTick = currentTick;
        m_lastSortColumn = m_sortColumn;
        m_lastSortOrder = m_sortOrder;
        m_lastSearchTerm = currentSearch;
        m_cacheValid = true;
    }

    return m_cachedDisplayData;
}

void ProvinceSimulationUI::rebuildDisplayCache() {
    m_cachedDisplayData.clear();

    uint32_t numProvinces = m_simulation->getNumProvinces();
    m_cachedDisplayData.reserve(numProvinces);

    const char* searchTerm = m_searchBuffer[0] != '\0' ? m_searchBuffer : nullptr;

    // Build display data with filtering
    for (uint32_t i = 0; i < numProvinces; ++i) {
        auto data = m_simulation->getProvinceData(i);
        auto initial = m_simulation->getInitialStats(i);

        float growth = ((data.population - initial.population) / initial.population) * 100.0f;

        ProvinceDisplayData display;
        display.index = i;
        display.population = data.population;
        display.populationGrowth = growth;
        display.foodProduction = data.foodProductionModifier;
        display.foodStorage = data.foodStorage;
        display.wealth = data.wealth;
        display.status = getStatusString(growth);

        if (!searchTerm || matchesSearch(display, searchTerm)) {
            m_cachedDisplayData.push_back(display);
        }
    }

    // Sort once
    sortProvinces(m_cachedDisplayData);
}

void ProvinceSimulationUI::sortProvinces(std::vector<ProvinceDisplayData>& provinces) {
    auto compare = [this](const ProvinceDisplayData& a, const ProvinceDisplayData& b) {
        bool result;
        switch (m_sortColumn) {
        case SortColumn::Index:
            result = a.index < b.index;
            break;
        case SortColumn::Population:
            result = a.population < b.population;
            break;
        case SortColumn::PopulationGrowth:
            result = a.populationGrowth < b.populationGrowth;
            break;
        case SortColumn::FoodProduction:
            result = a.foodProduction < b.foodProduction;
            break;
        case SortColumn::FoodStorage:
            result = a.foodStorage < b.foodStorage;
            break;
        case SortColumn::Wealth:
            result = a.wealth < b.wealth;
            break;
        }
        return m_sortOrder == SortOrder::Ascending ? result : !result;
        };

    std::sort(provinces.begin(), provinces.end(), compare);
}

bool ProvinceSimulationUI::matchesSearch(const ProvinceDisplayData& province, const char* searchTerm) {
    std::string indexStr = std::to_string(province.index);
    return indexStr.find(searchTerm) != std::string::npos;
}

void ProvinceSimulationUI::recordTickStats(uint32_t tickNumber, double elapsedMs) {
    TickStats stats;
    stats.tickNumber = tickNumber;
    stats.elapsedMs = elapsedMs;
    stats.totalPopulation = 0.0f;
    stats.totalWealth = 0.0f;
    stats.avgGrowth = 0.0f;
    stats.growing = 0;
    stats.stable = 0;
    stats.declining = 0;

    uint32_t numProvinces = m_simulation->getNumProvinces();

    for (uint32_t i = 0; i < numProvinces; ++i) {
        auto data = m_simulation->getProvinceData(i);
        auto initial = m_simulation->getInitialStats(i);

        stats.totalPopulation += data.population;
        stats.totalWealth += data.wealth;

        float growth = ((data.population - initial.population) / initial.population) * 100.0f;
        stats.avgGrowth += growth;

        if (growth > 5.0f) stats.growing++;
        else if (growth < -5.0f) stats.declining++;
        else stats.stable++;
    }

    stats.avgGrowth /= numProvinces;

    m_statsHistory.push_back(stats);

    if (m_statsHistory.size() > MAX_HISTORY) {
        m_statsHistory.erase(m_statsHistory.begin());
    }
}

const char* ProvinceSimulationUI::getStatusString(float growthPercent) {
    if (growthPercent > 5.0f) return "GROWING";
    if (growthPercent < -5.0f) return "DECLINING";
    return "STABLE";
}

ImVec4 ProvinceSimulationUI::getStatusColor(const char* status) {
    if (strcmp(status, "GROWING") == 0) return ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
    if (strcmp(status, "DECLINING") == 0) return ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
    return ImVec4(1.0f, 1.0f, 0.2f, 1.0f);
}

void ProvinceSimulationUI::loadCurrentSettings() {
    if (!m_simulation) return;

    m_settingsState.simParams = m_simulation->getSimulationParameters();
    m_settingsState.randParams = m_simulation->getRandomizationParameters();
    m_settingsChanged = false;
}

void ProvinceSimulationUI::applySettings() {
    if (!m_simulation) return;

    m_simulation->setSimulationParameters(m_settingsState.simParams);
    m_simulation->setRandomizationParameters(m_settingsState.randParams);
    m_settingsChanged = false;

    SPDLOG_INFO("Settings applied to simulation");
}

void ProvinceSimulationUI::resetSettingsToDefault() {
    m_settingsState.simParams = SimulationParameters();
    m_settingsState.randParams = RandomizationParameters();
    m_settingsChanged = true;
}

void ProvinceSimulationUI::startBenchmark() {
    if (!m_benchmarkGPU && !m_benchmarkCPU) {
        SPDLOG_WARN("No benchmark modes selected");
        return;
    }

    SPDLOG_INFO("Starting benchmark: {} ticks", m_benchmarkTickCount);

    m_benchmarkResults.clear();
    m_benchmarkRunning = true;
    m_benchmarkOriginalMode = m_simulation->getMode();
    m_benchmarkOriginalThreads = m_simulation->getCPUThreadCount();

    // Reset simulation to ensure fair comparison
    m_simulation->resetSimulation();

    // Start with GPU if enabled
    if (m_benchmarkGPU) {
        m_simulation->setMode(SimulationMode::GPU);
        m_benchmarkPhase = BenchmarkPhase::GPU_Running;
    }
    else {
        m_simulation->setMode(SimulationMode::CPU);
        m_simulation->setCPUThreadCount(m_benchmarkCPUThreads);
        m_benchmarkPhase = BenchmarkPhase::CPU_Running;
    }

    m_benchmarkStartTick = m_simulation->getCurrentTick();
    m_benchmarkStartTime = std::chrono::high_resolution_clock::now();

    m_simulation->runMultipleSteps(m_benchmarkTickCount);
}

void ProvinceSimulationUI::updateBenchmark() {
    if (!m_benchmarkRunning) return;

    // Wait for computation to finish
    if (m_simulation->isComputeInProgress()) return;

    uint32_t currentTick = m_simulation->getCurrentTick();
    uint32_t ticksCompleted = currentTick - m_benchmarkStartTick;

    // Check if current phase is complete
    if (ticksCompleted >= m_benchmarkTickCount) {
        completeBenchmarkPhase();
    }
}

void ProvinceSimulationUI::completeBenchmarkPhase() {
    auto endTime = std::chrono::high_resolution_clock::now();
    double totalTimeMs = std::chrono::duration<double, std::milli>(endTime - m_benchmarkStartTime).count();

    BenchmarkResult result;
    result.mode = m_simulation->getMode();
    result.cpuThreads = m_simulation->getCPUThreadCount();
    result.numTicks = m_benchmarkTickCount;
    result.numProvinces = m_simulation->getNumProvinces();
    result.totalTimeMs = totalTimeMs;
    result.avgTimePerTick = totalTimeMs / m_benchmarkTickCount;
    result.ticksPerSecond = (m_benchmarkTickCount * 1000.0) / totalTimeMs;

    m_benchmarkResults.push_back(result);

    SPDLOG_INFO("Benchmark phase complete: {} - {:.2f} ms total, {:.3f} ms/tick, {:.1f} ticks/sec",
        result.mode == SimulationMode::GPU ? "GPU" : "CPU",
        result.totalTimeMs, result.avgTimePerTick, result.ticksPerSecond);

    // Move to next phase
    switch (m_benchmarkPhase) {
    case BenchmarkPhase::GPU_Running:
        if (m_benchmarkCPU) {
            // Switch to CPU
            m_simulation->resetSimulation();
            m_simulation->setMode(SimulationMode::CPU);
            m_simulation->setCPUThreadCount(m_benchmarkCPUThreads);
            m_benchmarkPhase = BenchmarkPhase::CPU_Running;
            m_benchmarkStartTick = m_simulation->getCurrentTick();
            m_benchmarkStartTime = std::chrono::high_resolution_clock::now();
            m_simulation->runMultipleSteps(m_benchmarkTickCount);
        }
        else {
            finalizeBenchmark();
        }
        break;

    case BenchmarkPhase::CPU_Running:
        finalizeBenchmark();
        break;

    default:
        finalizeBenchmark();
    }
}

void ProvinceSimulationUI::finalizeBenchmark() {
    SPDLOG_INFO("Benchmark complete!");

    // Restore original mode
    m_simulation->setMode(m_benchmarkOriginalMode);
    if (m_benchmarkOriginalMode == SimulationMode::CPU) {
        m_simulation->setCPUThreadCount(m_benchmarkOriginalThreads);
    }
    m_simulation->resetSimulation();

    m_benchmarkRunning = false;
    m_benchmarkPhase = BenchmarkPhase::Idle;
}
