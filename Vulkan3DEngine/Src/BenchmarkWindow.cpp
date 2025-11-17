#include "BenchmarkWindow.h"

BenchmarkWindow::Config BenchmarkWindow::s_config;

void BenchmarkWindow::render(ProvinceSimulationTest* simulation, bool* showWindow) {
    if (!simulation) return;

    ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Performance Benchmark", showWindow)) {
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "GPU vs CPU Performance Test");
    ImGui::Separator();
    ImGui::Spacing();

    bool benchmarkRunning = simulation->isBenchmarkRunning();

    // Konfiguracja
    if (!benchmarkRunning) {
        renderConfiguration(simulation);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Progress
    if (benchmarkRunning) {
        renderProgress(simulation);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // Wyniki
    renderResults(simulation);

    ImGui::End();
}

void BenchmarkWindow::renderConfiguration(ProvinceSimulationTest* simulation) {
    bool canStart = !simulation->hasStepsRequested();

    ImGui::Text("Benchmark Configuration:");
    ImGui::Spacing();

    if (!canStart) ImGui::BeginDisabled();

    // Liczba prowincji
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Simulation Settings:");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(200);
    ImGui::InputInt("Number of Provinces", &s_config.numProvinces, 1024, 65536);
    if (s_config.numProvinces < 1024) s_config.numProvinces = 1024;
    if (s_config.numProvinces > 1048576) s_config.numProvinces = 1048576;

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Total number of provinces to simulate");
    }

    ImGui::SetNextItemWidth(200);
    ImGui::InputInt("Number of Ticks", &s_config.tickCount, 10, 100);
    if (s_config.tickCount < 10) s_config.tickCount = 10;
    if (s_config.tickCount > 100000) s_config.tickCount = 100000;

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("How many simulation steps to run");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Konfiguracja CPU
    ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "CPU Configuration:");
    ImGui::Spacing();

    ImGui::Checkbox("Benchmark CPU", &s_config.benchmarkCPU);

    ImGui::SetNextItemWidth(200);
    if (!s_config.benchmarkCPU) ImGui::BeginDisabled();
    ImGui::InputInt("CPU Threads", &s_config.cpuThreads, 1, 4);
    if (s_config.cpuThreads < 1) s_config.cpuThreads = 1;
    if (s_config.cpuThreads > 64) s_config.cpuThreads = 64;
    if (!s_config.benchmarkCPU) ImGui::EndDisabled();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Number of threads for parallel CPU processing");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Konfiguracja GPU
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "GPU Configuration:");
    ImGui::Spacing();

    ImGui::Checkbox("Benchmark GPU", &s_config.benchmarkGPU);

    ImGui::SetNextItemWidth(200);
    if (!s_config.benchmarkGPU) ImGui::BeginDisabled();
    ImGui::InputInt("GPU Readback Interval", &s_config.gpuReadbackInterval, 1, 10);
    if (s_config.gpuReadbackInterval < 1) s_config.gpuReadbackInterval = 1;
    if (s_config.gpuReadbackInterval > 100) s_config.gpuReadbackInterval = 100;
    if (!s_config.benchmarkGPU) ImGui::EndDisabled();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("How often to read data from GPU (in ticks)\n"
            "Higher values = less overhead, better performance");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (!s_config.benchmarkGPU && !s_config.benchmarkCPU) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
            "Please select at least one benchmark mode!");
    }
    else {
        if (ImGui::Button("Run Benchmark", ImVec2(200, 40))) {
            // Zastosuj konfigurację symulacji
            auto simParams = simulation->getSimulationParameters();
            simParams.numProvinces = s_config.numProvinces;

            auto randParams = simulation->getRandomizationParameters();

            simulation->setGPUReadbackInterval(s_config.gpuReadbackInterval);
            simulation->resetSimulationWithParameters(simParams, randParams);

            // Uruchom benchmark
            BenchmarkConfig config;
            config.benchmarkGPU = s_config.benchmarkGPU;
            config.benchmarkCPU = s_config.benchmarkCPU;
            config.numTicks = s_config.tickCount;
            config.cpuThreads = s_config.cpuThreads;

            simulation->startBenchmark(config);
        }
    }

    if (!canStart) ImGui::EndDisabled();

    ImGui::Spacing();

    if (!canStart) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
            "Cannot start benchmark while simulation is running");
    }
}

void BenchmarkWindow::renderProgress(ProvinceSimulationTest* simulation) {
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Benchmark Running...");
    ImGui::Spacing();

    const char* phase = simulation->getBenchmarkPhaseDescription();
    ImGui::Text("Current Phase: %s", phase);

    float progress = simulation->getBenchmarkProgress();
    ImGui::ProgressBar(progress, ImVec2(-1, 0));

    uint32_t currentTick = simulation->getCurrentTick();
    ImGui::Text("Current Tick: %u", currentTick);

    ImGui::Spacing();

    if (ImGui::Button("Cancel Benchmark", ImVec2(150, 0))) {
        simulation->cancelBenchmark();
    }
}

void BenchmarkWindow::renderResults(ProvinceSimulationTest* simulation) {
    const auto& results = simulation->getBenchmarkResults();

    if (results.empty()) {
        ImGui::TextDisabled("No benchmark results yet");
        ImGui::Text("Configure and run a benchmark to see results");
        return;
    }

    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Benchmark Results:");
    ImGui::Spacing();

    // Tabela wyników
    ImGuiTableFlags flags = ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("Results", 10, flags)) {
        ImGui::TableSetupColumn("Mode");
        ImGui::TableSetupColumn("Provinces");
        ImGui::TableSetupColumn("Threads");
        ImGui::TableSetupColumn("Ticks");
        ImGui::TableSetupColumn("Compute (ms)");
        ImGui::TableSetupColumn("Readback (ms)");
        ImGui::TableSetupColumn("Total Time (ms)");
        ImGui::TableSetupColumn("Time/Tick (ms)");
        ImGui::TableSetupColumn("Ticks/sec");
        ImGui::TableHeadersRow();

        for (const auto& result : results) {
            ImGui::TableNextRow();

            // Mode
            ImGui::TableSetColumnIndex(0);
            if (result.mode == SimulationMode::GPU) {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "GPU");
            }
            else {
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "CPU");
            }

            // Provinces
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", result.numProvinces);

            // Threads
            ImGui::TableSetColumnIndex(2);
            if (result.mode == SimulationMode::CPU) {
                ImGui::Text("%zu", result.cpuThreads);
            }
            else {
                ImGui::TextDisabled("-");
            }

            // Ticks
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u", result.numTicks);

            // Compute time
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2f", result.totalComputeMs);

            // Readback time
            ImGui::TableSetColumnIndex(5);
            if (result.totalReadbackMs > 0.0) {
                ImGui::Text("%.2f", result.totalReadbackMs);
            }
            else {
                ImGui::TextDisabled("-");
            }

            // Total time
            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%.2f", result.totalTimeMs);

            // Time per tick
            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%.3f", result.avgTimePerTick);

            // Ticks per second
            ImGui::TableSetColumnIndex(8);
            ImGui::Text("%.1f", result.ticksPerSecond);
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // Porównanie wydajności
    if (results.size() >= 2) {
        renderComparison(results);
    }

    ImGui::Spacing();

    if (ImGui::Button("Clear Results", ImVec2(150, 0))) {
        simulation->clearBenchmarkResults();
    }
}

void BenchmarkWindow::renderComparison(const std::vector<BenchmarkResult>& results) {
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Performance Comparison:");
    ImGui::Spacing();

    // Znajdź GPU i CPU
    const BenchmarkResult* gpuResult = nullptr;
    const BenchmarkResult* cpuResult = nullptr;

    for (const auto& result : results) {
        if (result.mode == SimulationMode::GPU && !gpuResult) {
            gpuResult = &result;
        }
        if (result.mode == SimulationMode::CPU && !cpuResult) {
            cpuResult = &result;
        }
    }

    if (!gpuResult || !cpuResult) {
        ImGui::TextDisabled("Run both GPU and CPU benchmarks to see comparison");
        return;
    }

    // Oblicz speedup
    double speedup = cpuResult->totalTimeMs / gpuResult->totalTimeMs;
    double throughputGPU = gpuResult->ticksPerSecond * gpuResult->numProvinces;
    double throughputCPU = cpuResult->ticksPerSecond * cpuResult->numProvinces;

    ImGui::Columns(2, "ComparisonColumns", false);

    // Kolumna 1: Speedup
    ImGui::Text("Speedup:");
    if (speedup > 1.0) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "GPU is %.2fx faster", speedup);
        ImGui::Text("(%.1f%% faster)", (speedup - 1.0) * 100.0);
    }
    else {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "CPU is %.2fx faster", 1.0 / speedup);
        ImGui::Text("(%.1f%% faster)", (1.0 / speedup - 1.0) * 100.0);
    }

    // Kolumna 2: Przepustowość
    ImGui::NextColumn();
    ImGui::Text("Throughput (provinces/sec):");
    ImGui::Text("GPU: %.2f million/sec", throughputGPU / 1000000.0);
    ImGui::Text("CPU: %.2f million/sec", throughputCPU / 1000000.0);

    ImGui::Columns(1);

    ImGui::Spacing();

    // Wizualizacja porównania
    ImGui::Text("Time per Tick Comparison:");

    float gpuTime = gpuResult->avgTimePerTick;
    float cpuTime = cpuResult->avgTimePerTick;
    float maxTime = std::max(gpuTime, cpuTime);

    ImGui::Text("GPU: ");
    ImGui::SameLine();
    ImGui::ProgressBar(gpuTime / maxTime, ImVec2(-1, 0),
        (std::to_string(gpuTime) + " ms").c_str());

    ImGui::Text("CPU: ");
    ImGui::SameLine();
    ImGui::ProgressBar(cpuTime / maxTime, ImVec2(-1, 0),
        (std::to_string(cpuTime) + " ms").c_str());

    ImGui::Spacing();

    // Dodatkowe szczegóły
    ImGui::Text("Configuration Details:");
    ImGui::Text("  Provinces: %u", gpuResult->numProvinces);
    ImGui::Text("  CPU Threads: %zu", cpuResult->cpuThreads);
}
