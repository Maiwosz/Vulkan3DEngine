#pragma once
#include "IProvinceSimulation.h"
#include "MaterialManager.h"
#include "ComputeDispatcher.h"
#include <vector>
#include <optional>
#include <memory>

/**
 * GPU-based province simulation using compute shaders
 */
class ProvinceSimulationGPU : public IProvinceSimulation {
public:
    explicit ProvinceSimulationGPU(ComputeDispatcher* dispatcher);
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
    MaterialSmartHandle material_;

    SimulationParameters simParams_;
    RandomizationParameters randParams_;

    // Cache buffers for UI display (CPU-side only)
    std::unique_ptr<ProvinceDataBuffer> displayCache_;
    std::vector<ProvinceData> initialStats_;

    uint32_t stepCounter_ = 0;
    uint32_t pendingSteps_ = 0;

    // Async operation tracking
    std::optional<ComputeTaskHandle> activeComputeTask_;
    std::optional<ShaderLib::AsyncOperationHandle> activeGPUReadOp_;

    enum class SimulationState {
        Idle,
        Computing,
        ReadingFromGPU,
        ReadyForNextStep
    };
    SimulationState state_ = SimulationState::Idle;

    // State machine
    void updateStateMachine();
    void handleComputingState();
    void handleReadingState();
    void handleReadyState();

    // Operations
    bool dispatchNextStep();
    void startGPURead();
    void updateDisplayCache();

    // Initialization helpers
    void initializeGPUData();
    void syncParametersToGPU();
};
