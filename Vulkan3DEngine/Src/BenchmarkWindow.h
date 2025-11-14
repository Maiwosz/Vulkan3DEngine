#pragma once
#include <imgui.h>
#include "ProvinceSimulationTest.h"

/**
 * Okno testów wydajnościowych GPU vs CPU
 * Z dodatkową konfiguracją readback interval i liczby prowincji
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
    };

    static Config s_config;

    static void renderConfiguration(ProvinceSimulationTest* simulation);
    static void renderProgress(ProvinceSimulationTest* simulation);
    static void renderResults(ProvinceSimulationTest* simulation);
    static void renderComparison(const std::vector<BenchmarkResult>& results);
};
