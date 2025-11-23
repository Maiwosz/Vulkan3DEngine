#include "SimulationSettingsWindow.h"

SimulationSettingsWindow::State SimulationSettingsWindow::s_state;
bool SimulationSettingsWindow::s_initialized = false;

void SimulationSettingsWindow::render(ProvinceSimulationTest* simulation, bool* showWindow) {
    if (!simulation) return;

    if (!s_initialized) {
        initialize(simulation);
    }

    ImGui::SetNextWindowSize(ImVec2(600, 650), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Simulation Settings", showWindow)) {
        ImGui::End();
        return;
    }

    bool canEdit = !simulation->hasStepsRequested();

    if (!canEdit) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
            "Settings locked during computation");
        ImGui::Separator();
        ImGui::BeginDisabled();
    }

    // Parametry symulacji
    renderSimulationParams();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Parametry randomizacji
    renderRandomizationParams();

    if (!canEdit) {
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Przyciski akcji
    renderActionButtons(simulation);

    if (s_state.modified) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
            "* Settings have been modified");
    }

    ImGui::End();
}

void SimulationSettingsWindow::initialize(ProvinceSimulationTest* simulation) {
    s_state.simParams = simulation->getSimulationParameters();
    s_state.randParams = simulation->getRandomizationParameters();
    s_state.modified = false;
    s_initialized = true;
}

void SimulationSettingsWindow::renderSimulationParams() {
    if (!ImGui::CollapsingHeader("Simulation Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Indent();

    // Liczba prowincji
    // Liczba prowincji - slider po potęgach dwójki
    ImGui::Text("Number of Provinces:");

    // Konwersja wartości na indeks potęgi dwójki
    auto valueToLog2 = [](int value) -> int {
        return (int)std::round(std::log2((double)value));
        };

    auto log2ToValue = [](int log2) -> int {
        return 1 << log2;
        };

    int minLog2 = 10;  // 2^10 = 1024
    int maxLog2 = 20;  // 2^20 = 1048576
    int currentLog2 = valueToLog2(s_state.simParams.numProvinces);

    ImGui::SetNextItemWidth(350);
    if (ImGui::SliderInt("##ProvinceSlider", &currentLog2, minLog2, maxLog2)) {
        s_state.simParams.numProvinces = log2ToValue(currentLog2);
        s_state.modified = true;
    }

    // Custom format - wyświetl wartość i potęgę
    ImGui::SameLine();
    ImGui::Text("%u (2^%d)", s_state.simParams.numProvinces, currentLog2);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Drag to select power of 2\nRange: 1,024 (2^10) to 1,048,576 (2^20)");
    }

    ImGui::Spacing();

    // Parametry ekonomiczne
    if (ImGui::DragFloat("Food Consumption per Pop",
        &s_state.simParams.foodConsumptionPerPop,
        0.01f, 0.01f, 1.0f, "%.3f")) {
        s_state.modified = true;
    }

    if (ImGui::DragFloat("Base Population Growth",
        &s_state.simParams.basePopulationGrowth,
        0.001f, 0.0f, 0.1f, "%.4f")) {
        s_state.modified = true;
    }

    if (ImGui::DragFloat("Starvation Threshold",
        &s_state.simParams.starvationThreshold,
        0.05f, 0.0f, 1.0f, "%.2f")) {
        s_state.modified = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Food ratio below which population starts declining");
    }

    if (ImGui::DragFloat("Wealth per Population",
        &s_state.simParams.wealthPerPop,
        0.05f, 0.0f, 10.0f, "%.2f")) {
        s_state.modified = true;
    }

    // POPRAWKA: Max Food Storage - teraz uint32_t
    int maxFoodStorage = static_cast<int>(s_state.simParams.maxFoodStorage);
    if (ImGui::DragInt("Max Food Storage",
        &maxFoodStorage,
        1, 10, 10000)) {
        s_state.simParams.maxFoodStorage = static_cast<uint32_t>(maxFoodStorage);
        s_state.modified = true;
    }

    // POPRAWKA: Min Population - teraz uint32_t (w tysiącach)
    int minPopulation = static_cast<int>(s_state.simParams.minPopulation);
    if (ImGui::DragInt("Min Population (thousands)",
        &minPopulation,
        1, 1, 100)) {
        s_state.simParams.minPopulation = static_cast<uint32_t>(minPopulation);
        s_state.modified = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Minimum viable population in thousands");
    }

    ImGui::Unindent();
}

void SimulationSettingsWindow::renderRandomizationParams() {
    if (!ImGui::CollapsingHeader("Initial Randomization", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Indent();

    // POPRAWKA: Populacja - teraz uint32_t
    ImGui::Text("Initial Population Range (thousands):");
    int minPop = static_cast<int>(s_state.randParams.minPopulation);
    if (ImGui::DragInt("Min##Pop", &minPop,
        1, 1, 1000)) {
        s_state.randParams.minPopulation = static_cast<uint32_t>(minPop);
        if (s_state.randParams.minPopulation > s_state.randParams.maxPopulation) {
            s_state.randParams.maxPopulation = s_state.randParams.minPopulation;
        }
        s_state.modified = true;
    }

    int maxPop = static_cast<int>(s_state.randParams.maxPopulation);
    if (ImGui::DragInt("Max##Pop", &maxPop,
        1, 1, 1000)) {
        s_state.randParams.maxPopulation = static_cast<uint32_t>(maxPop);
        if (s_state.randParams.maxPopulation < s_state.randParams.minPopulation) {
            s_state.randParams.minPopulation = s_state.randParams.maxPopulation;
        }
        s_state.modified = true;
    }

    ImGui::Spacing();

    // Produkcja żywności - to pozostaje float
    ImGui::Text("Food Production Range:");
    if (ImGui::DragFloat("Min##Food", &s_state.randParams.minFoodProduction,
        0.1f, 0.1f, 50.0f, "%.2f")) {
        if (s_state.randParams.minFoodProduction > s_state.randParams.maxFoodProduction) {
            s_state.randParams.maxFoodProduction = s_state.randParams.minFoodProduction;
        }
        s_state.modified = true;
    }

    if (ImGui::DragFloat("Max##Food", &s_state.randParams.maxFoodProduction,
        0.1f, 0.1f, 50.0f, "%.2f")) {
        if (s_state.randParams.maxFoodProduction < s_state.randParams.minFoodProduction) {
            s_state.randParams.minFoodProduction = s_state.randParams.maxFoodProduction;
        }
        s_state.modified = true;
    }

    ImGui::Spacing();

    // POPRAWKA: Początkowy zapas żywności - teraz uint32_t
    int initialFood = static_cast<int>(s_state.randParams.initialFoodStorage);
    if (ImGui::DragInt("Initial Food Storage",
        &initialFood,
        1, 0, 1000)) {
        s_state.randParams.initialFoodStorage = static_cast<uint32_t>(initialFood);
        s_state.modified = true;
    }

    ImGui::Spacing();

    // Seed - to już było int
    int seed = s_state.randParams.randomSeed;
    if (ImGui::InputInt("Random Seed (0 = random)", &seed, 1, 100)) {
        if (seed < 0) seed = 0;
        s_state.randParams.randomSeed = seed;
        s_state.modified = true;
    }

    ImGui::Unindent();
}

void SimulationSettingsWindow::renderActionButtons(ProvinceSimulationTest* simulation) {
    bool canApply = !simulation->hasStepsRequested();

    if (!canApply) ImGui::BeginDisabled();

    // Zastosuj bez resetu
    if (ImGui::Button("Apply (Live)", ImVec2(150, 0))) {
        simulation->setSimulationParameters(s_state.simParams);
        simulation->setRandomizationParameters(s_state.randParams);
        s_state.modified = false;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Apply settings without resetting simulation");
    }

    ImGui::SameLine();

    // Zastosuj z resetem
    if (ImGui::Button("Apply & Reset", ImVec2(150, 0))) {
        simulation->resetSimulationWithParameters(s_state.simParams, s_state.randParams);
        s_state.modified = false;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Apply settings and restart simulation from beginning");
    }

    if (!canApply) ImGui::EndDisabled();

    ImGui::SameLine();

    // Reset do domyślnych
    if (ImGui::Button("Reset to Defaults", ImVec2(150, 0))) {
        s_state.simParams = SimulationParameters();
        s_state.randParams = RandomizationParameters();
        s_state.modified = true;
    }

    ImGui::SameLine();

    // Przeładuj aktualne
    if (ImGui::Button("Reload Current", ImVec2(150, 0))) {
        initialize(simulation);
    }
}
