#pragma once
#include <imgui.h>
#include "ProvinceSimulationTest.h"

/**
 * Okno konfiguracji parametrów symulacji
 * (bez wyboru trybu - przeniesiony do głównego widoku)
 */
class SimulationSettingsWindow {
public:
    static void render(ProvinceSimulationTest* simulation, bool* showWindow);

private:
    struct State {
        SimulationParameters simParams;
        RandomizationParameters randParams;
        bool modified = false;
    };

    static State s_state;
    static bool s_initialized;

    static void initialize(ProvinceSimulationTest* simulation);
    static void renderSimulationParams();
    static void renderRandomizationParams();
    static void renderActionButtons(ProvinceSimulationTest* simulation);
};
