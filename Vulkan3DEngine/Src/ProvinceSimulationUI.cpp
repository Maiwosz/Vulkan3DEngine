#include "ProvinceSimulationUI.h"
#include "SimulationSettingsWindow.h"
#include "BenchmarkWindow.h"
#include "IImGuiProvider.h"
#include "Engine.h"

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
            SPDLOG_INFO("ImGui callback registered");
        }
    }
}

void ProvinceSimulationUI::OnUpdate(float deltaTime) {
    if (m_simulation) {
        uint32_t currentTick = m_simulation->getCurrentTick();
        if (currentTick > m_perfAccumulators.lastProcessedTick) {
            auto stats = m_simulation->getPerformanceStats();
            if (stats.lastTimings.totalMs > 0) {
                m_perfAccumulators.addSample(
                    stats.lastTimings.computeMs,
                    stats.lastTimings.readbackMs,
                    stats.lastTimings.totalMs
                );
                m_perfAccumulators.lastProcessedTick = currentTick;
            }
        }
    }
}

void ProvinceSimulationUI::OnDestroy() {
    SPDLOG_INFO("ProvinceSimulationUI destroyed");
    m_simulation = nullptr;
}

void ProvinceSimulationUI::setSimulation(ProvinceSimulationTest* simulation) {
    m_simulation = simulation;
    SPDLOG_INFO("Simulation reference set in UI");
}

void ProvinceSimulationUI::renderScriptUI() {
    ImGui::Text("Province Simulation UI");
    ImGui::Checkbox("Show Window", &m_showMainWindow);
}

// =============================================================================
// GŁÓWNE RENDEROWANIE
// =============================================================================

void ProvinceSimulationUI::render() {
    if (!m_simulation) return;

    if (m_showMainWindow) {
        renderMainWindow();
    }

    if (m_showSettingsWindow) {
        SimulationSettingsWindow::render(m_simulation, &m_showSettingsWindow);
    }

    if (m_showBenchmarkWindow) {
        BenchmarkWindow::render(m_simulation, &m_showBenchmarkWindow);
    }
}

void ProvinceSimulationUI::renderMainWindow() {
    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Province Simulation", &m_showMainWindow)) {
        ImGui::End();
        return;
    }

    // Selektor trybu i konfiguracji
    renderModeSelector();

    ImGui::Spacing();

    // Przyciski sterowania
    renderControlButtons();

    ImGui::Separator();
    ImGui::Spacing();

    // Główna zawartość - 3 sekcje
    float contentHeight = ImGui::GetContentRegionAvail().y;

    // Panel wydajności - 30% wysokości
    ImGui::BeginChild("Performance", ImVec2(0, contentHeight * 0.3f), true);
    renderPerformancePanel();
    ImGui::EndChild();

    ImGui::Spacing();

    // Dolne sekcje - 65% wysokości
    ImGui::BeginChild("BottomSection", ImVec2(0, 0), false);
    {
        float width = ImGui::GetContentRegionAvail().x;

        // Statystyki po lewej - 55% szerokości
        ImGui::BeginChild("Statistics", ImVec2(width * 0.55f, 0), true);
        renderStatisticsPanel();
        ImGui::EndChild();

        ImGui::SameLine();

        // Inspektor prowincji po prawej - 45% szerokości
        ImGui::BeginChild("Inspector", ImVec2(0, 0), true);
        renderProvinceInspector();
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::End();
}

// =============================================================================
// SELEKTOR TRYBU
// =============================================================================

void ProvinceSimulationUI::renderModeSelector() {
    // Nie pozwalaj na zmianę trybu podczas benchmarku!
    bool isComputing = m_simulation->hasStepsRequested();
    bool isBenchmarkRunning = m_simulation->isBenchmarkRunning();
    bool canChangeMode = !isComputing && !isBenchmarkRunning;

    SimulationMode currentMode = m_simulation->getMode();

    // Przycisk przełączania trybu
    if (!canChangeMode) ImGui::BeginDisabled();

    const char* modeButtonLabel = currentMode == SimulationMode::GPU ? "GPU Mode" : "CPU Mode";
    ImVec4 modeButtonColor = currentMode == SimulationMode::GPU
        ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f)
        : ImVec4(0.2f, 0.6f, 1.0f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, modeButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(modeButtonColor.x * 1.2f, modeButtonColor.y * 1.2f, modeButtonColor.z * 1.2f, 1.0f));

    if (ImGui::Button(modeButtonLabel, ImVec2(120, 30))) {
        // Ta sekcja będzie disabled podczas benchmarku, więc to się nie wykona
        SimulationMode newMode = currentMode == SimulationMode::GPU
            ? SimulationMode::CPU
            : SimulationMode::GPU;
        m_simulation->setMode(newMode);
        SPDLOG_INFO("Mode switched to: {}", newMode == SimulationMode::GPU ? "GPU" : "CPU");
    }

    ImGui::PopStyleColor(2);

    if (!canChangeMode) {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (isBenchmarkRunning) {
                ImGui::SetTooltip("Cannot change mode during benchmark");
            }
            else {
                ImGui::SetTooltip("Cannot change mode while simulation is running");
            }
        }
    }
    else {
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Click to switch between GPU and CPU mode");
        }
    }

    ImGui::SameLine();

    // Konfiguracja specyficzna dla trybu - też disabled podczas benchmarku
    if (currentMode == SimulationMode::CPU) {
        // CPU: Liczba wątków
        ImGui::Text("CPU Threads:");
        ImGui::SameLine();

        int cpuThreads = (int)m_simulation->getCPUThreadCount();
        ImGui::SetNextItemWidth(100);

        if (!canChangeMode) ImGui::BeginDisabled();

        if (ImGui::SliderInt("##CPUThreads", &cpuThreads, 1, 64)) {
            m_simulation->setCPUThreadCount(cpuThreads);
        }

        if (!canChangeMode) ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Number of CPU threads for parallel processing");
        }
    }
    else {
        // GPU: Readback interval
        ImGui::Text("GPU Readback:");
        ImGui::SameLine();

        int readbackInterval = (int)m_simulation->getGPUReadbackInterval();
        ImGui::SetNextItemWidth(100);

        if (!canChangeMode) ImGui::BeginDisabled();

        if (ImGui::SliderInt("##GPUReadback", &readbackInterval, 1, 100)) {
            m_simulation->setGPUReadbackInterval(readbackInterval);
        }

        if (!canChangeMode) ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("How often to read data from GPU (in ticks)\nHigher = less UI updates, better GPU performance");
        }
    }

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20, 0));
    ImGui::SameLine();

    if (currentMode == SimulationMode::GPU) {
        bool autoReadback = m_simulation->isAutoReadback();

        if (!canChangeMode) ImGui::BeginDisabled();

        if (ImGui::Checkbox("Auto Readback", &autoReadback)) {
            m_simulation->setAutoReadback(autoReadback);
        }

        if (!canChangeMode) ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Automatically readback data from GPU\nDisable for manual control");
        }

        if (!autoReadback) {
            ImGui::SameLine();
            if (ImGui::Button("Manual Readback")) {
                m_simulation->triggerManualReadback();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Trigger manual GPU->CPU readback");
            }
        }

        // NOWA SEKCJA - Full Data Readback
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20, 0));
        ImGui::SameLine();

        bool fullReadback = m_simulation->isGPUFullDataReadback();

        if (!canChangeMode) ImGui::BeginDisabled();

        if (ImGui::Checkbox("Full Data", &fullReadback)) {
            m_simulation->setGPUFullDataReadback(fullReadback);
        }

        if (!canChangeMode) ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Read all province data from GPU\n"
                "Slower but uses CPU aggregation\n"
                "Disable for GPU-only (faster)");
        }
    }

    // Status i tick - w prawym górnym rogu
    float windowWidth = ImGui::GetWindowWidth();
    ImGui::SameLine();

    // Oblicz szerokość dla maks. 1,000,000 ticków + padding
    float tickTextWidth = ImGui::CalcTextSize("Tick: 1000000").x;
    float offsetX = windowWidth - tickTextWidth - 20.0f; // 20px padding z prawej

    ImGui::SetCursorPosX(offsetX);
    ImGui::Text("Tick: %u", m_simulation->getCurrentTick());

    // Statusy pod tickiem - bliżej, zmniejszony spacing
    float currentY = ImGui::GetCursorPosY();
    ImGui::SetCursorPosY(currentY - 15.0f); // 15px bliżej ticku
    ImGui::SetCursorPosX(offsetX);

    if (isComputing) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[COMPUTING]");
    }
    else {
        ImGui::Dummy(ImVec2(ImGui::CalcTextSize("[COMPUTING]").x, ImGui::GetTextLineHeight()));
    }

    if (isBenchmarkRunning) {
        if (isComputing) {
            ImGui::SameLine();
        }
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[BENCHMARK]");
    }
}

// =============================================================================
// PRZYCISKI STEROWANIA
// =============================================================================

void ProvinceSimulationUI::renderControlButtons() {
    // FIXED: Sprawdź czy faktycznie są kroki do wykonania
    bool isComputing = m_simulation->hasStepsRequested();

    if (isComputing) ImGui::BeginDisabled();

    if (ImGui::Button("Single Step", ImVec2(120, 0))) {
        m_simulation->runSingleStep();
    }

    ImGui::SameLine();

    static int tickCount = 100;
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("##Ticks", &tickCount, 10, 100);
    if (tickCount < 1) tickCount = 1;
    if (tickCount > 100000) tickCount = 100000;

    ImGui::SameLine();
    if (ImGui::Button("Run N Ticks", ImVec2(120, 0))) {
        m_simulation->runMultipleSteps(tickCount);
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(100, 0))) {
        m_simulation->resetSimulation();
        m_perfAccumulators.reset();
        m_populationHistory.clear();
        m_wealthHistory.clear();
    }

    if (isComputing) ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20, 0));

    ImGui::SameLine();
    if (ImGui::Button("Settings", ImVec2(100, 0))) {
        m_showSettingsWindow = !m_showSettingsWindow;
    }

    ImGui::SameLine();
    if (ImGui::Button("Benchmark", ImVec2(100, 0))) {
        m_showBenchmarkWindow = !m_showBenchmarkWindow;
    }
}

// =============================================================================
// PANEL WYDAJNOŚCI
// =============================================================================

void ProvinceSimulationUI::renderPerformancePanel() {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Performance Metrics");
    ImGui::Separator();
    ImGui::Spacing();

    auto stats = m_simulation->getPerformanceStats();

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);

    // Current tick metrics
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "Current Tick:");
    ImGui::Columns(4, "CurrentTickColumns", false);

    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%.3f ms", stats.lastTimings.computeMs);
    ImGui::Text("Compute");

    ImGui::NextColumn();
    if (stats.lastTimings.readbackMs > 0.0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "%.3f ms", stats.lastTimings.readbackMs);
    }
    else {
        ImGui::TextDisabled("-");
    }
    ImGui::Text("Readback");

    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%.3f ms", stats.lastTimings.totalMs);
    ImGui::Text("Total");

    ImGui::NextColumn();
    float ticksPerSec = stats.lastTimings.totalMs > 0 ? 1000.0f / stats.lastTimings.totalMs : 0.0f;
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%.1f", ticksPerSec);
    ImGui::Text("Ticks/Sec");

    ImGui::Columns(1);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Average metrics
    if (m_perfAccumulators.sampleCount > 0) {
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 1.0f, 1.0f), "Average (over %u ticks):",
            m_perfAccumulators.sampleCount);

        ImGui::Columns(4, "AvgTickColumns", false);

        double avgCompute = m_perfAccumulators.totalComputeMs / m_perfAccumulators.sampleCount;
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%.3f ms", avgCompute);
        ImGui::Text("Compute");

        ImGui::NextColumn();
        double avgReadback = m_perfAccumulators.totalReadbackMs / m_perfAccumulators.sampleCount;
        if (avgReadback > 0.0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "%.3f ms", avgReadback);
        }
        else {
            ImGui::TextDisabled("-");
        }
        ImGui::Text("Readback");

        ImGui::NextColumn();
        double avgTotal = m_perfAccumulators.totalTimeMs / m_perfAccumulators.sampleCount;
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%.3f ms", avgTotal);
        ImGui::Text("Total");

        ImGui::NextColumn();
        float avgTicksPerSec = avgTotal > 0 ? 1000.0f / avgTotal : 0.0f;
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%.1f", avgTicksPerSec);
        ImGui::Text("Ticks/Sec");

        ImGui::Columns(1);
    }
    else {
        ImGui::TextDisabled("No performance data yet - run a tick first");
    }

    ImGui::PopFont();
}

// =============================================================================
// PANEL STATYSTYK
// =============================================================================

void ProvinceSimulationUI::renderStatisticsPanel() {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Simulation Statistics");
    ImGui::Separator();
    ImGui::Spacing();

    const auto* latest = m_simulation->getLatestStatistics();

    if (!latest) {
        ImGui::TextDisabled("No simulation data yet - run a tick first");
        return;
    }

    // Główne statystyki
    ImGui::Text("Total Population: %.2f million", latest->totalPopulation / 1000.0f);
    ImGui::Text("Total Wealth: %.1f", latest->totalWealth);
    ImGui::Text("Average Growth: %.2f%%", latest->avgGrowth);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Rozkład statusów prowincji
    ImGui::Text("Province Status Distribution:");

    uint32_t total = latest->growing + latest->stable + latest->declining;
    if (total > 0) {
        ImGui::Columns(3, "StatusCols", false);

        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Growing");
        ImGui::Text("%u (%.1f%%)", latest->growing,
            (float)latest->growing / total * 100.0f);

        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "Stable");
        ImGui::Text("%u (%.1f%%)", latest->stable,
            (float)latest->stable / total * 100.0f);

        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Declining");
        ImGui::Text("%u (%.1f%%)", latest->declining,
            (float)latest->declining / total * 100.0f);

        ImGui::Columns(1);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ZMIENIONE: Historia populacji z adaptywną kompresją
    const auto& history = m_simulation->getStatisticsHistory();
    if (!history.empty()) {
        // Dodaj nowy punkt do historii
        uint32_t currentTick = history.back().tickNumber;
        float currentPop = history.back().totalPopulation / 1000.0f;

        if (m_populationHistory.empty() ||
            m_populationHistory.back().tickNumber != currentTick) {

            m_populationHistory.push_back({ currentTick, currentPop });

            // Jeśli przekroczyliśmy limit, skompresuj
            if (m_populationHistory.size() > MAX_POPULATION_HISTORY) {
                compressPopulationHistory();
            }
        }

        // Przygotuj dane do wykresu
        std::vector<float> popHistory;
        popHistory.reserve(m_populationHistory.size());
        for (const auto& point : m_populationHistory) {
            popHistory.push_back(point.population);
        }

        ImGui::Text("Population History (millions) - %zu points:", popHistory.size());
        ImGui::PlotLines("##PopHistory",
            popHistory.data(),
            popHistory.size(),
            0,
            nullptr,
            FLT_MAX,
            FLT_MAX,
            ImVec2(0, 100));

        // Zmiana od początku
        if (!m_populationHistory.empty()) {
            float initialPop = m_populationHistory.front().population;
            float change = ((currentPop - initialPop) / initialPop) * 100.0f;
            ImGui::Text("Total Change: %+.2f%% from start (tick %u to %u)",
                change,
                m_populationHistory.front().tickNumber,
                m_populationHistory.back().tickNumber);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // NOWE: Historia bogactwa z adaptywną kompresją
    if (!history.empty()) {
        // Dodaj nowy punkt do historii
        uint32_t currentTick = history.back().tickNumber;
        float currentWealth = history.back().totalWealth;

        if (m_wealthHistory.empty() ||
            m_wealthHistory.back().tickNumber != currentTick) {

            m_wealthHistory.push_back({ currentTick, currentWealth });

            // Jeśli przekroczyliśmy limit, skompresuj
            if (m_wealthHistory.size() > MAX_WEALTH_HISTORY) {
                compressWealthHistory();
            }
        }

        // Przygotuj dane do wykresu
        std::vector<float> wealthHistoryData;
        wealthHistoryData.reserve(m_wealthHistory.size());
        for (const auto& point : m_wealthHistory) {
            wealthHistoryData.push_back(point.wealth);
        }

        ImGui::Text("Wealth History - %zu points:", wealthHistoryData.size());
        ImGui::PlotLines("##WealthHistory",
            wealthHistoryData.data(),
            wealthHistoryData.size(),
            0,
            nullptr,
            FLT_MAX,
            FLT_MAX,
            ImVec2(0, 100));

        // Zmiana od początku
        if (!m_wealthHistory.empty()) {
            float initialWealth = m_wealthHistory.front().wealth;
            float change = ((currentWealth - initialWealth) / initialWealth) * 100.0f;
            ImGui::Text("Total Change: %+.2f%% from start (tick %u to %u)",
                change,
                m_wealthHistory.front().tickNumber,
                m_wealthHistory.back().tickNumber);
        }
    }
}

// =============================================================================
// INSPEKTOR PROWINCJI
// =============================================================================

void ProvinceSimulationUI::renderProvinceInspector() {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Province Inspector");
    ImGui::Separator();
    ImGui::Spacing();

    uint32_t maxId = m_simulation->getNumProvinces() - 1;

    ImGui::Text("Enter Province ID (0 - %u):", maxId);
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("##ProvinceID", m_inspectBuffer, sizeof(m_inspectBuffer),
        ImGuiInputTextFlags_CharsDecimal)) {
        int id = atoi(m_inspectBuffer);
        if (id >= 0 && id <= (int)maxId) {
            m_inspectProvinceId = id;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Random")) {
        m_inspectProvinceId = rand() % (maxId + 1);
        snprintf(m_inspectBuffer, sizeof(m_inspectBuffer), "%d", m_inspectProvinceId);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Wyświetl dane prowincji
    if (m_inspectProvinceId >= 0 && m_inspectProvinceId <= (int)maxId) {
        auto current = m_simulation->getProvinceData(m_inspectProvinceId);
        auto initial = m_simulation->getInitialStats(m_inspectProvinceId);

        ImGui::Text("Province #%d", m_inspectProvinceId);
        ImGui::Spacing();

        bool wasEditMode = m_editMode;
        ImGui::Checkbox("Edit Mode", &m_editMode);

        if (m_editMode && !wasEditMode) {
            // Population and storage are now integers
            snprintf(m_editPopulation, sizeof(m_editPopulation), "%u", current.population);
            snprintf(m_editFoodProduction, sizeof(m_editFoodProduction), "%.2f", current.foodProductionModifier);
            snprintf(m_editWealth, sizeof(m_editWealth), "%u", current.wealth);
            snprintf(m_editFoodStorage, sizeof(m_editFoodStorage), "%u", current.foodStorage);
        }

        ImGui::Spacing();

        if (m_editMode) {
            // EDIT MODE
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Editing Province:");
            ImGui::Spacing();

            ImGui::Text("Population (k):");
            ImGui::SameLine(180);
            ImGui::SetNextItemWidth(150);
            ImGui::InputText("##EditPop", m_editPopulation, sizeof(m_editPopulation),
                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AllowTabInput);

            ImGui::Text("Food Production:");
            ImGui::SameLine(180);
            ImGui::SetNextItemWidth(150);
            ImGui::InputText("##EditFood", m_editFoodProduction, sizeof(m_editFoodProduction),
                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AllowTabInput);

            ImGui::Text("Wealth:");
            ImGui::SameLine(180);
            ImGui::SetNextItemWidth(150);
            ImGui::InputText("##EditWealth", m_editWealth, sizeof(m_editWealth),
                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AllowTabInput);

            ImGui::Text("Food Storage:");
            ImGui::SameLine(180);
            ImGui::SetNextItemWidth(150);
            ImGui::InputText("##EditStorage", m_editFoodStorage, sizeof(m_editFoodStorage),
                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AllowTabInput);

            ImGui::Spacing();

            if (ImGui::Button("Apply Changes", ImVec2(120, 0))) {
                ProvinceData newData;
                newData.population = std::max(1u, (uint32_t)atoi(m_editPopulation));
                newData.foodProductionModifier = std::max(0.0f, (float)atof(m_editFoodProduction));
                newData.wealth = std::max(0u, (uint32_t)atoi(m_editWealth));
                newData.foodStorage = std::max(0u, (uint32_t)atoi(m_editFoodStorage));

                m_simulation->setProvinceData(m_inspectProvinceId, newData);
                SPDLOG_INFO("Province {} data updated", m_inspectProvinceId);
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_editMode = false;
            }
        }
        else {
            // VIEW MODE
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "Current State:");
            ImGui::Text("  Population: %u k", current.population);  // %u for uint32_t
            ImGui::Text("  Food Production: %.2f /tick", current.foodProductionModifier);
            ImGui::Text("  Food Storage: %u", current.foodStorage);  // %u for uint32_t
            ImGui::Text("  Wealth: %u", current.wealth);  // %u for uint32_t

            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Initial State:");
            ImGui::Text("  Population: %u k", initial.population);  // %u for uint32_t
            ImGui::Text("  Food Production: %.2f /tick", initial.foodProductionModifier);

            ImGui::Spacing();

            // Calculate changes with proper integer arithmetic
            int32_t popChange = static_cast<int32_t>(current.population) -
                static_cast<int32_t>(initial.population);
            float popChangePercent = (static_cast<float>(popChange) /
                static_cast<float>(initial.population)) * 100.0f;

            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Changes:");
            ImGui::Text("  Population: %+d k (%+.2f%%)", popChange, popChangePercent);
            ImGui::Text("  Wealth Gained: %u", current.wealth);

            ImGui::Spacing();

            const char* status = getStatusText(popChangePercent);
            ImGui::Text("Status: ");
            ImGui::SameLine();
            ImGui::TextColored(getStatusColor(popChangePercent), "%s", status);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Food balance
            auto params = m_simulation->getSimulationParameters();
            float consumption = static_cast<float>(current.population) * params.foodConsumptionPerPop;
            float balance = current.foodProductionModifier - consumption;

            ImGui::Text("Food Balance:");
            ImGui::Text("  Production: %.2f", current.foodProductionModifier);
            ImGui::Text("  Consumption: %.2f", consumption);
            ImGui::Text("  Balance: ");
            ImGui::SameLine();
            if (balance >= 0) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "+%.2f (Surplus)", balance);
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%.2f (Deficit)", balance);
            }
        }
    }
}

// =============================================================================
// FUNKCJE POMOCNICZE
// =============================================================================

ImVec4 ProvinceSimulationUI::getStatusColor(float growthPercent) {
    if (growthPercent > 5.0f) return ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
    if (growthPercent < -5.0f) return ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
    return ImVec4(1.0f, 1.0f, 0.2f, 1.0f);
}

const char* ProvinceSimulationUI::getStatusText(float growthPercent) {
    if (growthPercent > 5.0f) return "GROWING";
    if (growthPercent < -5.0f) return "DECLINING";
    return "STABLE";
}

void ProvinceSimulationUI::compressPopulationHistory() {
    if (m_populationHistory.size() <= 3) return; // Potrzebujemy przynajmniej 3 punktów

    size_t removeIdx = findLeastSignificantPoint();

    if (removeIdx != SIZE_MAX) {
        m_populationHistory.erase(m_populationHistory.begin() + removeIdx);
        SPDLOG_DEBUG("Compressed population history: removed point at index {} (tick {})",
            removeIdx, m_populationHistory[removeIdx].tickNumber);
    }
}

size_t ProvinceSimulationUI::findLeastSignificantPoint() const {
    if (m_populationHistory.size() <= 3) return SIZE_MAX;

    size_t leastSignificantIdx = SIZE_MAX;
    float minSignificance = std::numeric_limits<float>::max();

    // Nie usuwamy pierwszego ani ostatniego punktu
    for (size_t i = 1; i < m_populationHistory.size() - 1; ++i) {
        const auto& prev = m_populationHistory[i - 1];
        const auto& curr = m_populationHistory[i];
        const auto& next = m_populationHistory[i + 1];

        // Oblicz interpolowaną wartość bez tego punktu
        float tickRange = static_cast<float>(next.tickNumber - prev.tickNumber);
        float tickOffset = static_cast<float>(curr.tickNumber - prev.tickNumber);
        float interpolatedValue = prev.population +
            (next.population - prev.population) * (tickOffset / tickRange);

        // Błąd interpolacji jako miara istotności punktu
        float error = std::abs(curr.population - interpolatedValue);

        // Dodatkowo: preferuj usuwanie punktów gęsto rozmieszczonych
        float densityPenalty = 1.0f / (tickRange + 1.0f);
        float significance = error + densityPenalty * 0.1f;

        if (significance < minSignificance) {
            minSignificance = significance;
            leastSignificantIdx = i;
        }
    }

    return leastSignificantIdx;
}

void ProvinceSimulationUI::compressWealthHistory() {
    if (m_wealthHistory.size() <= 3) return;

    size_t removeIdx = findLeastSignificantWealthPoint();

    if (removeIdx != SIZE_MAX) {
        m_wealthHistory.erase(m_wealthHistory.begin() + removeIdx);
        SPDLOG_DEBUG("Compressed wealth history: removed point at index {} (tick {})",
            removeIdx, m_wealthHistory[removeIdx].tickNumber);
    }
}

size_t ProvinceSimulationUI::findLeastSignificantWealthPoint() const {
    if (m_wealthHistory.size() <= 3) return SIZE_MAX;

    size_t leastSignificantIdx = SIZE_MAX;
    float minSignificance = std::numeric_limits<float>::max();

    for (size_t i = 1; i < m_wealthHistory.size() - 1; ++i) {
        const auto& prev = m_wealthHistory[i - 1];
        const auto& curr = m_wealthHistory[i];
        const auto& next = m_wealthHistory[i + 1];

        float tickRange = static_cast<float>(next.tickNumber - prev.tickNumber);
        float tickOffset = static_cast<float>(curr.tickNumber - prev.tickNumber);
        float interpolatedValue = prev.wealth +
            (next.wealth - prev.wealth) * (tickOffset / tickRange);

        float error = std::abs(curr.wealth - interpolatedValue);
        float densityPenalty = 1.0f / (tickRange + 1.0f);
        float significance = error + densityPenalty * 0.1f;

        if (significance < minSignificance) {
            minSignificance = significance;
            leastSignificantIdx = i;
        }
    }

    return leastSignificantIdx;
}
