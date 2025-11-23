#pragma once
#include <imgui.h>
#include "ProvinceSimulationTest.h"

/**
 * Okno testów wydajnościowych GPU vs CPU
 * Z dodatkowymi testami zbieżności numerycznej
 */
class BenchmarkWindow {
public:
    static void render(ProvinceSimulationTest* simulation, bool* showWindow);

private:
    struct Config {
        int tickCount = 100;
        int cpuThreads = 8;
        int gpuReadbackInterval = 1;
        int numProvinces = 65536;
        bool benchmarkGPU = true;
        bool benchmarkCPU = true;
        bool gpuFullDataReadback = false;
        bool testConvergence = true;
        int convergenceProvinceIndex = 0;  // 0 = auto
    };

    static Config s_config;

    static void renderConfiguration(ProvinceSimulationTest* simulation);
    static void renderProgress(ProvinceSimulationTest* simulation);
    static void renderResults(ProvinceSimulationTest* simulation);
    static void renderComparison(const std::vector<BenchmarkResult>& results);

    // Convergence rendering
    static void renderConvergenceComparison(const ConvergenceComparison& conv);
    static void renderTickComparison(const char* label,
        const ConvergenceComparison::TickComparison& comp);
    static void renderConvergenceSummary(const ConvergenceComparison& conv);

    // Helper for colored error display
    static void renderErrorCell(float error);
    static void renderErrorCell(double error);
};
