#include "BenchmarkWindow.h"
#include <cmath>

BenchmarkWindow::Config BenchmarkWindow::s_config;

void BenchmarkWindow::render(ProvinceSimulationTest* simulation, bool* showWindow) {
    if (!simulation) return;

    ImGui::SetNextWindowSize(ImVec2(900, 800), ImGuiCond_FirstUseEver);
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

    // Wyniki wydajności
    renderResults(simulation);

    // NOWE: Convergence comparison
    const auto& convergence = simulation->getConvergenceComparison();
    if (convergence.has_value()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        renderConvergenceComparison(convergence.value());
    }

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

    ImGui::Text("Number of Provinces:");
    ImGui::SetNextItemWidth(400);

    auto valueToLog2 = [](int value) -> int {
        return (int)std::round(std::log2((double)value));
        };

    auto log2ToValue = [](int log2) -> int {
        return 1 << log2;
        };

    int minLog2 = 10;  // 2^10 = 1024
    int maxLog2 = 20;  // 2^20 = 1048576
    int currentLog2 = valueToLog2(s_config.numProvinces);

    if (ImGui::SliderInt("##ProvinceSlider", &currentLog2, minLog2, maxLog2)) {
        s_config.numProvinces = log2ToValue(currentLog2);
    }

    ImGui::SameLine();
    ImGui::Text("%d (2^%d)", s_config.numProvinces, currentLog2);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Drag to select power of 2\nRange: 1,024 (2^10) to 1,048,576 (2^20)");
    }

    ImGui::Spacing();

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

    ImGui::SetNextItemWidth(200);
    if (!s_config.benchmarkGPU) ImGui::BeginDisabled();
    ImGui::Checkbox("GPU Full Data Readback", &s_config.gpuFullDataReadback);
    if (!s_config.benchmarkGPU) ImGui::EndDisabled();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Read all province data from GPU for CPU aggregation\n"
            "Slower but tests full readback performance\n"
            "Disable for GPU-only aggregation (faster)");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // NOWE: Convergence testing
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Convergence Testing:");
    ImGui::Spacing();

    ImGui::Checkbox("Test Numerical Convergence", &s_config.testConvergence);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Compare CPU and GPU results to detect numerical differences\n"
            "Tests if both implementations produce identical results");
    }

    ImGui::SetNextItemWidth(200);
    if (!s_config.testConvergence) ImGui::BeginDisabled();
    ImGui::InputInt("Province to Compare", &s_config.convergenceProvinceIndex, 100, 1000);
    if (s_config.convergenceProvinceIndex < 0) s_config.convergenceProvinceIndex = 0;
    if (!s_config.testConvergence) ImGui::EndDisabled();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Which province to compare in detail\n"
            "0 = auto-select (middle province)");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (!s_config.benchmarkGPU && !s_config.benchmarkCPU) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
            "Please select at least one benchmark mode!");
    }
    else {
        if (ImGui::Button("Run Benchmark", ImVec2(200, 40))) {
            auto simParams = simulation->getSimulationParameters();
            simParams.numProvinces = s_config.numProvinces;

            auto randParams = simulation->getRandomizationParameters();

            simulation->resetSimulationWithParameters(simParams, randParams);
            simulation->setGPUReadbackInterval(s_config.gpuReadbackInterval);
            simulation->setGPUFullDataReadback(s_config.gpuFullDataReadback);

            BenchmarkConfig config;
            config.benchmarkGPU = s_config.benchmarkGPU;
            config.benchmarkCPU = s_config.benchmarkCPU;
            config.numTicks = s_config.tickCount;
            config.cpuThreads = s_config.cpuThreads;
            config.testConvergence = s_config.testConvergence;
            config.convergenceProvinceIndex = s_config.convergenceProvinceIndex;

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

            ImGui::TableSetColumnIndex(0);
            if (result.mode == SimulationMode::GPU) {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "GPU");
            }
            else {
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "CPU");
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", result.numProvinces);

            ImGui::TableSetColumnIndex(2);
            if (result.mode == SimulationMode::CPU) {
                ImGui::Text("%zu", result.cpuThreads);
            }
            else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u", result.numTicks);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2f", result.totalComputeMs);

            ImGui::TableSetColumnIndex(5);
            if (result.totalReadbackMs > 0.0) {
                ImGui::Text("%.2f", result.totalReadbackMs);
            }
            else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%.2f", result.totalTimeMs);

            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%.3f", result.avgTimePerTick);

            ImGui::TableSetColumnIndex(8);
            ImGui::Text("%.1f", result.ticksPerSecond);
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();

    if (results.size() >= 2) {
        renderComparison(results);
    }

    ImGui::Spacing();

    if (ImGui::Button("Clear Results", ImVec2(150, 0))) {
        simulation->clearBenchmarkResults();
        simulation->clearConvergenceComparison();
    }
}

void BenchmarkWindow::renderComparison(const std::vector<BenchmarkResult>& results) {
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Performance Comparison:");
    ImGui::Spacing();

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

    double speedup = cpuResult->totalTimeMs / gpuResult->totalTimeMs;
    double throughputGPU = gpuResult->ticksPerSecond * gpuResult->numProvinces;
    double throughputCPU = cpuResult->ticksPerSecond * cpuResult->numProvinces;

    ImGui::Columns(2, "ComparisonColumns", false);

    ImGui::Text("Speedup:");
    if (speedup > 1.0) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "GPU is %.2fx faster", speedup);
        ImGui::Text("(%.1f%% faster)", (speedup - 1.0) * 100.0);
    }
    else {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "CPU is %.2fx faster", 1.0 / speedup);
        ImGui::Text("(%.1f%% faster)", (1.0 / speedup - 1.0) * 100.0);
    }

    ImGui::NextColumn();
    ImGui::Text("Throughput (provinces/sec):");
    ImGui::Text("GPU: %.2f million/sec", throughputGPU / 1000000.0);
    ImGui::Text("CPU: %.2f million/sec", throughputCPU / 1000000.0);

    ImGui::Columns(1);

    ImGui::Spacing();

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

    ImGui::Text("Configuration Details:");
    ImGui::Text("  Provinces: %u", gpuResult->numProvinces);
    ImGui::Text("  CPU Threads: %zu", cpuResult->cpuThreads);
}

// NOWA FUNKCJA: Renderowanie porównania zbieżności
void BenchmarkWindow::renderConvergenceComparison(const ConvergenceComparison& conv) {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Numerical Convergence Analysis:");
    ImGui::Spacing();

    // Overall status
    if (conv.hasSignificantDivergence) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
            "WARNING: Significant divergence detected!");
        ImGui::Text("Max relative error: %.6f%%", conv.maxRelativeError * 100.0);
    }
    else {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
            "Results are numerically consistent");
        ImGui::Text("Max relative error: %.8f%%", conv.maxRelativeError * 100.0);
    }

    ImGui::Spacing();
    ImGui::Text("Compared province: #%u", conv.comparedProvinceIndex);
    ImGui::Spacing();

    // Tabs dla różnych ticków
    if (ImGui::BeginTabBar("ConvergenceTabs")) {

        // First Tick
        if (ImGui::BeginTabItem("First Tick")) {
            renderTickComparison("First Tick", conv.firstTick);
            ImGui::EndTabItem();
        }

        // Last Tick
        if (ImGui::BeginTabItem("Last Tick")) {
            renderTickComparison("Last Tick", conv.lastTick);
            ImGui::EndTabItem();
        }

        // Summary
        if (ImGui::BeginTabItem("Summary")) {
            renderConvergenceSummary(conv);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}
void BenchmarkWindow::renderTickComparison(
    const char* label,
    const ConvergenceComparison::TickComparison& comp)
{
    ImGui::Spacing();
    // Province Data Comparison
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Province #%u:",
        comp.cpuProvince.provinceIndex);
    ImGui::Spacing();
    ImGuiTableFlags flags = ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("ProvinceComparison", 5, flags)) {
        ImGui::TableSetupColumn("Metric");
        ImGui::TableSetupColumn("CPU");
        ImGui::TableSetupColumn("GPU");
        ImGui::TableSetupColumn("Abs Error");
        ImGui::TableSetupColumn("Rel Error (%)");
        ImGui::TableHeadersRow();

        // Population
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Population");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", comp.cpuProvince.data.population);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%u", comp.gpuProvince.data.population);
        ImGui::TableSetColumnIndex(3);
        renderErrorCell(comp.provinceErrors.populationAbsError);
        ImGui::TableSetColumnIndex(4);
        renderErrorCell(comp.provinceErrors.populationRelError * 100.0f);

        // Wealth
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Wealth");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", comp.cpuProvince.data.wealth);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%u", comp.gpuProvince.data.wealth);
        ImGui::TableSetColumnIndex(3);
        renderErrorCell(comp.provinceErrors.wealthAbsError);
        ImGui::TableSetColumnIndex(4);
        renderErrorCell(comp.provinceErrors.wealthRelError * 100.0f);

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Aggregate Data Comparison
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Aggregate Statistics:");
    ImGui::Spacing();

    if (ImGui::BeginTable("AggregateComparison", 5, flags)) {
        ImGui::TableSetupColumn("Metric");
        ImGui::TableSetupColumn("CPU");
        ImGui::TableSetupColumn("GPU");
        ImGui::TableSetupColumn("Abs Error");
        ImGui::TableSetupColumn("Rel Error (%)");
        ImGui::TableHeadersRow();

        // Total Population
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Total Population");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", comp.cpuAggregate.totalPopulation);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%u", comp.gpuAggregate.totalPopulation);
        ImGui::TableSetColumnIndex(3);
        renderErrorCell(comp.aggregateErrors.populationAbsError);
        ImGui::TableSetColumnIndex(4);
        renderErrorCell(comp.aggregateErrors.populationRelError * 100.0);

        // Total Wealth
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Total Wealth");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", comp.cpuAggregate.totalWealth);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%u", comp.gpuAggregate.totalWealth);
        ImGui::TableSetColumnIndex(3);
        renderErrorCell(comp.aggregateErrors.wealthAbsError);
        ImGui::TableSetColumnIndex(4);
        renderErrorCell(comp.aggregateErrors.wealthRelError * 100.0);

        // Avg Growth
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Avg Growth");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.6f", comp.cpuAggregate.avgGrowth);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.6f", comp.gpuAggregate.avgGrowth);
        ImGui::TableSetColumnIndex(3);
        renderErrorCell(comp.aggregateErrors.growthAbsError);
        ImGui::TableSetColumnIndex(4);
        renderErrorCell(comp.aggregateErrors.growthRelError * 100.0f);

        // Province counts
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Growing / Stable / Declining");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u / %u / %u",
            comp.cpuAggregate.growing,
            comp.cpuAggregate.stable,
            comp.cpuAggregate.declining);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%u / %u / %u",
            comp.gpuAggregate.growing,
            comp.gpuAggregate.stable,
            comp.gpuAggregate.declining);
        ImGui::TableSetColumnIndex(3);
        ImGui::TextDisabled("-");
        ImGui::TableSetColumnIndex(4);
        ImGui::TextDisabled("-");

        ImGui::EndTable();
    }
}

void BenchmarkWindow::renderConvergenceSummary(const ConvergenceComparison& conv) {
    ImGui::Spacing();

    ImGui::Text("Error Evolution:");
    ImGui::Spacing();

    // Porównanie błędów między pierwszym a ostatnim tickiem
    auto compareErrors = [](const char* metric,
        float firstError,
        float lastError) {
            ImGui::Text("%s:", metric);
            ImGui::Indent();
            ImGui::Text("First tick:  %.8f%%", firstError * 100.0f);
            ImGui::Text("Last tick:   %.8f%%", lastError * 100.0f);

            if (lastError > firstError * 1.5f) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                    "Error increased by %.1f%%",
                    ((lastError / firstError) - 1.0f) * 100.0f);
            }
            else if (lastError < firstError * 0.5f) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
                    "Error decreased by %.1f%%",
                    (1.0f - (lastError / firstError)) * 100.0f);
            }
            else {
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Error stable");
            }
            ImGui::Unindent();
            ImGui::Spacing();
        };

    compareErrors("Province Population Error",
        conv.firstTick.provinceErrors.populationRelError,
        conv.lastTick.provinceErrors.populationRelError);

    compareErrors("Province Wealth Error",
        conv.firstTick.provinceErrors.wealthRelError,
        conv.lastTick.provinceErrors.wealthRelError);

    compareErrors("Aggregate Population Error",
        static_cast<float>(conv.firstTick.aggregateErrors.populationRelError),
        static_cast<float>(conv.lastTick.aggregateErrors.populationRelError));

    compareErrors("Aggregate Wealth Error",
        static_cast<float>(conv.firstTick.aggregateErrors.wealthRelError),
        static_cast<float>(conv.lastTick.aggregateErrors.wealthRelError));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Overall assessment
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "Assessment:");
    ImGui::Spacing();

    if (conv.maxRelativeError < 0.0001) {  // < 0.01%
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
            "Excellent: Nearly identical results (< 0.01%% error)");
    }
    else if (conv.maxRelativeError < 0.001) {  // < 0.1%
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.2f, 1.0f),
            "Good: Minor floating-point differences (< 0.1%% error)");
    }
    else if (conv.maxRelativeError < 0.01) {  // < 1%
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
            "Acceptable: Some numerical drift (< 1%% error)");
    }
    else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
            "Warning: Significant divergence detected!");
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "Note: Small differences are expected due to floating-point precision, "
        "different operation orders, and compiler optimizations. "
        "Errors below 0.1%% are typically acceptable for this type of simulation.");
}

void BenchmarkWindow::renderErrorCell(float error) {
    if (error < 0.0001f) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%.8f", error);
    }
    else if (error < 0.001f) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%.8f", error);
    }
    else if (error < 0.01f) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.8f", error);
    }
    else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%.8f", error);
    }
}

void BenchmarkWindow::renderErrorCell(double error) {
    if (error < 0.0001) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%.8f", error);
    }
    else if (error < 0.001) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%.8f", error);
    }
    else if (error < 0.01) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.8f", error);
    }
    else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%.8f", error);
    }
}
