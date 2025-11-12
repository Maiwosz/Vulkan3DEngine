#pragma once
#include "IProvinceSimulation.h"
#include "MaterialManager.h"
#include "ComputeDispatcher.h"
#include "AsyncMemoryOps.h"
#include <vector>
#include <optional>
#include <memory>

/**
 * GPU-based province simulation using compute shaders
 * Uses unified structures and AsyncMemoryOps for efficient data transfer
 */
class ProvinceSimulationGPU : public IProvinceSimulation {
public:
    explicit ProvinceSimulationGPU(ComputeDispatcher* dispatcher, AsyncMemoryOps* asyncMemOps = nullptr);
    ~ProvinceSimulationGPU() override;

    // IProvinceSimulation interface
    bool initialize(const SimulationParameters& simParams,
        const RandomizationParameters& randParams) override;

    void runSingleStep() override;
    void runMultipleSteps(uint32_t numSteps) override;
    void reset() override;

    bool isComputeInProgress() const override;
    uint32_t getCurrentTick() const override { return stepCounter_; }

    ProvinceData getProvinceData(uint32_t index) const override;
    ProvinceData getInitialStats(uint32_t index) const override;

    const SimulationParameters& getSimulationParameters() const override { return simParams_; }
    const RandomizationParameters& getRandomizationParameters() const override { return randParams_; }
    void setSimulationParameters(const SimulationParameters& params) override;
    void setRandomizationParameters(const RandomizationParameters& params) override;

    void requestDataRefresh() override;
    bool isDataRefreshComplete() const override;

    const char* getTypeName() const override { return "GPU"; }

    // GPU-specific
    void update(); // Must be called from main thread
    void setMaterial(MaterialSmartHandle material) { material_ = material; }

private:
    ComputeDispatcher* computeDispatcher_;
    AsyncMemoryOps* asyncMemOps_;
    MaterialSmartHandle material_;

    SimulationParameters simParams_;
    RandomizationParameters randParams_;

    // Unified buffer structure
    std::unique_ptr<ProvinceDataBuffer> workingBuffer_;
    std::vector<ProvinceData> cachedStats_;
    std::vector<ProvinceData> initialStats_;

    uint32_t stepCounter_ = 0;
    uint32_t pendingSteps_ = 0;

    std::optional<ComputeTaskHandle> activeComputeTask_;
    std::vector<BufferSyncTaskHandle> activeSyncTasks_;

    enum class SimulationState {
        Idle,
        Computing,
        SyncingFromGPU,
        ReadyForNextStep
    };
    SimulationState state_ = SimulationState::Idle;

    void updateStateMachine();
    void handleComputingState();
    void handleSyncingState();
    void handleReadyState();

    bool dispatchNextStep();
    void syncDataFromGPU();
    void updateCachedData();
    void syncParametersToMaterial();
};
