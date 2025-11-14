#pragma once
#include "ISimulationStrategy.h"
#include "MaterialManager.h"
#include "ComputeDispatcher.h"
#include <atomic>

/**
 * GPU Strategy - Synchronous execution
 *
 * Flow per step:
 * 1. Start timer
 * 2. Dispatch compute (dispatch + waitForTask)
 * 3. Readback to CPU (CopyFromGPUDirect) - only every N ticks
 * 4. Stop timer
 * 5. Increment tick
 *
 * All operations BLOCK - simple and accurate timing.
 */
class GPUSimulationStrategy : public ISimulationStrategy {
public:
    GPUSimulationStrategy(
        ComputeDispatcher* dispatcher,
        MaterialManager* materialMgr
    );
    ~GPUSimulationStrategy() override;

    bool initialize(
        const SimulationParameters& simParams,
        const RandomizationParameters& randParams,
        ProvinceDataBuffer* sharedBuffer
    ) override;

    void shutdown() override;

    // BLOCKING - returns when step completes
    void executeSingleStep() override;

    uint32_t getCurrentTick() const override {
        return tickCounter_.load(std::memory_order_relaxed);
    }

    double getLastStepTimeMs() const override {
        return lastStepTimeMs_.load(std::memory_order_relaxed);
    }

    const char* getTypeName() const override {
        return "GPU";
    }

    // Configuration
    void setReadbackInterval(uint32_t interval) {
        readbackInterval_ = std::max(1u, interval);
    }

    uint32_t getReadbackInterval() const {
        return readbackInterval_;
    }

private:
    void initializeGPUData(const RandomizationParameters& randParams);
    void syncParametersToGPU();
    void readbackFromGPU();

    ComputeDispatcher* dispatcher_;
    MaterialManager* materialManager_;
    MaterialSmartHandle material_;

    SimulationParameters simParams_;
    ProvinceDataBuffer* sharedBuffer_;

    std::atomic<uint32_t> tickCounter_{ 0 };
    std::atomic<double> lastStepTimeMs_{ 0.0 };

    uint32_t readbackInterval_{ 1 };
    uint32_t ticksSinceReadback_{ 0 };
};
