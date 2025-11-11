#pragma once
#include "CppScriptBase.h"
#include <spdlog/spdlog.h>
#include "MaterialManager.h"
#include "ComputeDispatcher.h"
#include "Engine.h"
#include <vector>
#include <optional>

struct ProvinceStats {
    float population;
    float foodProductionModifier;
    float wealth;
    float foodStorage;
};

struct SimulationParameters {
    uint32_t numProvinces = 65536;
    float foodConsumptionPerPop = 0.1f;
    float basePopulationGrowth = 0.02f;
    float starvationThreshold = 0.5f;
    float wealthPerPop = 0.5f;
    float maxFoodStorage = 100.0f;
    float minPopulation = 0.1f;
};

struct RandomizationParameters {
    float minPopulation = 5.0f;
    float maxPopulation = 50.0f;
    float minFoodProduction = 1.0f;
    float maxFoodProduction = 10.0f;
    float initialFoodStorage = 10.0f;
    uint32_t randomSeed = 0; // 0 = use random_device
};

/**
 * ProvinceSimulationTest - Async compute simulation with smart caching
 *
 * Improvements:
 * - Uses new async dispatch API (dispatchForDataSize -> ComputeTaskHandle)
 * - Uses new async sync API (SyncFromGPUAsync -> BufferSyncTaskHandle)
 * - Caches ComputeDispatcher reference
 * - GPU sync only after compute completion (not per getProvinceData call)
 * - Maintains CPU-side cache of province data
 * - Non-blocking operations throughout
 * - Customizable simulation and randomization parameters
 */
class ProvinceSimulationTest : public CppScriptBase {
public:
    const char* getScriptName() const override;
    void OnCreate() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;

    // ========================================================================
    // PUBLIC API FOR UI
    // ========================================================================

    bool isSimulationRunning() const { return simulationRunning_; }
    uint32_t getCurrentTick() const { return stepCounter_; }
    uint32_t getNumProvinces() const { return simParams_.numProvinces; }
    float getFoodConsumptionPerPop() const { return simParams_.foodConsumptionPerPop; }

    // Check if any compute or sync operation is in progress
    bool isComputeInProgress() const;

    // Get cached province data (fast, no GPU sync)
    ProvinceStats getProvinceData(uint32_t index) const;
    ProvinceStats getInitialStats(uint32_t index) const;

    // Get/Set parameters
    const SimulationParameters& getSimulationParameters() const { return simParams_; }
    const RandomizationParameters& getRandomizationParameters() const { return randParams_; }

    void setSimulationParameters(const SimulationParameters& params);
    void setRandomizationParameters(const RandomizationParameters& params);

    // ========================================================================
    // SIMULATION CONTROL (All async)
    // ========================================================================

    void runSingleStep();
    void runMultipleSteps(uint32_t numSteps);
    void resetSimulation(); // Uses current parameters
    void resetSimulationWithParameters(const SimulationParameters& simParams,
        const RandomizationParameters& randParams);

    // ========================================================================
    // MANUAL CACHE REFRESH (if needed by UI)
    // ========================================================================

    // Request fresh data from GPU (async, non-blocking)
    void requestDataRefresh();

    // Check if refresh is complete
    bool isDataRefreshComplete() const;

private:
    // ========================================================================
    // INTERNAL STATE
    // ========================================================================

    MaterialSmartHandle material_;
    bool simulationRunning_ = false;
    uint32_t stepCounter_ = 0;
    uint32_t pendingSteps_ = 0;

    std::vector<ProvinceStats> initialStats_;
    std::vector<ProvinceStats> cachedStats_;  // CPU-side cache
    SimulationParameters simParams_;
    RandomizationParameters randParams_;

    // Cached dispatcher reference
    ComputeDispatcher* computeDispatcher_ = nullptr;

    // Task tracking
    std::optional<ComputeTaskHandle> activeComputeTask_;
    std::vector<BufferSyncTaskHandle> activeSyncTasks_;

    // State machine for async operations
    enum class SimulationState {
        Idle,                   // No operations in progress
        Computing,              // Compute shader executing
        SyncingFromGPU,        // Reading results from GPU
        ReadyForNextStep       // Data cached, ready for next step
    };
    SimulationState state_ = SimulationState::Idle;

    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================

    void initializeSimulation();
    void updateStateMachine();

    // State handlers
    void handleComputingState();
    void handleSyncingState();
    void handleReadyState();

    // Cache management
    void syncDataFromGPU();           // Start async sync
    void updateCachedData();          // Update cache after sync completes

    // Compute dispatch
    bool dispatchNextStep();

    // Material synchronization
    void syncParametersToMaterial();
};
