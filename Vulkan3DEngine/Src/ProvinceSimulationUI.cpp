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

    m_tickTimeHistory.reserve(PERF_HISTORY_SIZE);
}

void ProvinceSimulationUI::OnUpdate(float deltaTime) {
    if (m_simulation) {
        // Aktualizuj historię wydajności
        auto stats = m_simulation->getPerformanceStats();
        if (stats.lastTimings.totalMs > 0) {
            addTickTimeToHistory(stats.lastTimings.totalMs);
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

    // Panel wydajności - 40% wysokości
    ImGui::BeginChild("Performance", ImVec2(0, contentHeight * 0.4f), true);
    renderPerformancePanel();
    ImGui::EndChild();

    ImGui::Spacing();

    // Dolne sekcje - 55% wysokości
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
    }

    // Status i tick
    ImGui::Text("Tick: %u", m_simulation->getCurrentTick());

    if (isComputing) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[COMPUTING]");
    }

    if (isBenchmarkRunning) {
        ImGui::SameLine();
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
        m_tickTimeHistory.clear();
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
    auto simParams = m_simulation->getSimulationParameters();

    // Kluczowe metryki w dużej czcionce
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Domyślna czcionka

    ImGui::Columns(4, "PerfColumns", false);

    // Kolumna 1: Compute time
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%.3f ms", stats.lastTimings.computeMs);
    ImGui::Text("Compute/Tick");

    // Kolumna 2: Readback time
    ImGui::NextColumn();
    if (stats.lastTimings.readbackMs > 0.0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "%.3f ms", stats.lastTimings.readbackMs);
    }
    else {
        ImGui::TextDisabled("-");
    }
    ImGui::Text("Readback/Tick");

    // Kolumna 3: Total time
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%.3f ms", stats.lastTimings.totalMs);
    ImGui::Text("Total/Tick");

    // Kolumna 4: Ticks/sec (existing)
    ImGui::NextColumn();
    float ticksPerSec = stats.lastTimings.totalMs > 0 ? 1000.0f / stats.lastTimings.totalMs : 0.0f;
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%.1f", ticksPerSec);
    ImGui::Text("Ticks/Second");

    ImGui::Columns(1);
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Wykres czasu wykonania
    if (!m_tickTimeHistory.empty()) {
        ImGui::Text("Tick Time History:");
        ImGui::PlotLines("##TickTime",
            m_tickTimeHistory.data(),
            m_tickTimeHistory.size(),
            0,
            nullptr,
            0.0f,
            FLT_MAX,
            ImVec2(0, 120));

        // Min/Max/Avg
        float minTime = *std::min_element(m_tickTimeHistory.begin(), m_tickTimeHistory.end());
        float maxTime = *std::max_element(m_tickTimeHistory.begin(), m_tickTimeHistory.end());
        float avgTime = 0.0f;
        for (float t : m_tickTimeHistory) avgTime += t;
        avgTime /= m_tickTimeHistory.size();

        ImGui::Text("Min: %.3f ms  |  Max: %.3f ms  |  Avg: %.3f ms",
            minTime, maxTime, avgTime);
    }
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

    // Historia populacji
    const auto& history = m_simulation->getStatisticsHistory();
    if (history.size() > 1) {
        std::vector<float> popHistory;
        popHistory.reserve(std::min(history.size(), size_t(200)));

        size_t start = history.size() > 200 ? history.size() - 200 : 0;
        for (size_t i = start; i < history.size(); ++i) {
            popHistory.push_back(history[i].totalPopulation / 1000.0f);
        }

        ImGui::Text("Population History (millions):");
        ImGui::PlotLines("##PopHistory",
            popHistory.data(),
            popHistory.size(),
            0,
            nullptr,
            FLT_MAX,
            FLT_MAX,
            ImVec2(0, 100));

        // Zmiana od początku
        float initialPop = history.front().totalPopulation;
        float currentPop = latest->totalPopulation;
        float change = ((currentPop - initialPop) / initialPop) * 100.0f;

        ImGui::Text("Total Change: %+.2f%% from start", change);
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

        // Toggle Edit Mode
        bool wasEditMode = m_editMode;
        ImGui::Checkbox("Edit Mode", &m_editMode);

        if (m_editMode && !wasEditMode) {
            // Entering edit mode - populate buffers
            snprintf(m_editPopulation, sizeof(m_editPopulation), "%.2f", current.population);
            snprintf(m_editFoodProduction, sizeof(m_editFoodProduction), "%.2f", current.foodProductionModifier);
            snprintf(m_editWealth, sizeof(m_editWealth), "%.2f", current.wealth);
            snprintf(m_editFoodStorage, sizeof(m_editFoodStorage), "%.2f", current.foodStorage);
        }

        ImGui::Spacing();

        if (m_editMode) {
            // EDIT MODE - Editable fields
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
                newData.population = std::max(0.1f, (float)atof(m_editPopulation));
                newData.foodProductionModifier = std::max(0.0f, (float)atof(m_editFoodProduction));
                newData.wealth = std::max(0.0f, (float)atof(m_editWealth));
                newData.foodStorage = std::max(0.0f, (float)atof(m_editFoodStorage));

                m_simulation->setProvinceData(m_inspectProvinceId, newData);
                SPDLOG_INFO("Province {} data updated", m_inspectProvinceId);
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_editMode = false;
            }
        }
        else {
            // VIEW MODE - Display current state
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "Current State:");
            ImGui::Text("  Population: %.2f k", current.population);
            ImGui::Text("  Food Production: %.2f /tick", current.foodProductionModifier);
            ImGui::Text("  Food Storage: %.1f", current.foodStorage);
            ImGui::Text("  Wealth: %.1f", current.wealth);

            ImGui::Spacing();

            // Stan początkowy
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Initial State:");
            ImGui::Text("  Population: %.2f k", initial.population);
            ImGui::Text("  Food Production: %.2f /tick", initial.foodProductionModifier);

            ImGui::Spacing();

            // Zmiany
            float popChange = current.population - initial.population;
            float popChangePercent = (popChange / initial.population) * 100.0f;

            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Changes:");
            ImGui::Text("  Population: %+.2f k (%+.2f%%)",
                popChange, popChangePercent);
            ImGui::Text("  Wealth Gained: %.1f", current.wealth);

            ImGui::Spacing();

            // Status
            const char* status = getStatusText(popChangePercent);
            ImGui::Text("Status: ");
            ImGui::SameLine();
            ImGui::TextColored(getStatusColor(popChangePercent), "%s", status);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Bilans żywności
            auto params = m_simulation->getSimulationParameters();
            float consumption = current.population * params.foodConsumptionPerPop;
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

void ProvinceSimulationUI::addTickTimeToHistory(float timeMs) {
    m_tickTimeHistory.push_back(timeMs);
    if (m_tickTimeHistory.size() > PERF_HISTORY_SIZE) {
        m_tickTimeHistory.erase(m_tickTimeHistory.begin());
    }
}

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
