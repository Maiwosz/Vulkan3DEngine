#pragma once
#include "CppScriptBase.h"
#include <spdlog/spdlog.h>
#include "IProvinceSimulation.h"
#include "ProvinceSimulationGPU.h"
#include "ProvinceSimulationCPU.h"
#include "MaterialManager.h"
#include "ComputeDispatcher.h"
#include "ThreadPool.h"
#include "AsyncMemoryOps.h"
#include "Engine.h"
#include <memory>

enum class SimulationMode {
    GPU,
    CPU
};

/**
 * Manager class that delegates to GPU or CPU implementations
 * Provides unified interface for UI
 */
class ProvinceSimulationTest : public CppScriptBase {
public:
    const char* getScriptName() const override;
    void OnCreate() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;

    // Mode switching
    void setMode(SimulationMode mode);
    SimulationMode getMode() const { return currentMode_; }

    // CPU-specific
    void setCPUThreadCount(size_t threads);
    size_t getCPUThreadCount() const;

    // Delegated interface
    bool isSimulationRunning() const { return simulation_ != nullptr; }
    uint32_t getCurrentTick() const { return simulation_ ? simulation_->getCurrentTick() : 0; }
    uint32_t getNumProvinces() const { return simParams_.numProvinces; }
    float getFoodConsumptionPerPop() const { return simParams_.foodConsumptionPerPop; }

    bool isComputeInProgress() const { return simulation_ ? simulation_->isComputeInProgress() : false; }

    ProvinceData getProvinceData(uint32_t index) const {
        return simulation_ ? simulation_->getProvinceData(index) : ProvinceData{ 0,0,0,0 };
    }
    ProvinceData getInitialStats(uint32_t index) const {
        return simulation_ ? simulation_->getInitialStats(index) : ProvinceData{ 0,0,0,0 };
    }

    const SimulationParameters& getSimulationParameters() const { return simParams_; }
    const RandomizationParameters& getRandomizationParameters() const { return randParams_; }

    void setSimulationParameters(const SimulationParameters& params);
    void setRandomizationParameters(const RandomizationParameters& params);

    void runSingleStep() { if (simulation_) simulation_->runSingleStep(); }
    void runMultipleSteps(uint32_t numSteps) { if (simulation_) simulation_->runMultipleSteps(numSteps); }
    void resetSimulation();
    void resetSimulationWithParameters(const SimulationParameters& simParams,
        const RandomizationParameters& randParams);

    void requestDataRefresh() { if (simulation_) simulation_->requestDataRefresh(); }
    bool isDataRefreshComplete() const { return simulation_ ? simulation_->isDataRefreshComplete() : true; }

private:
    SimulationMode currentMode_ = SimulationMode::GPU;
    std::unique_ptr<IProvinceSimulation> simulation_;

    SimulationParameters simParams_;
    RandomizationParameters randParams_;

    // Resources
    ComputeDispatcher* computeDispatcher_ = nullptr;
    ThreadPool* threadPool_ = nullptr;
    MaterialManager* materialManager_ = nullptr;
    std::unique_ptr<AsyncMemoryOps> asyncMemoryOps_;

    void initializeSimulation();
    void createGPUSimulation();
    void createCPUSimulation();
};
