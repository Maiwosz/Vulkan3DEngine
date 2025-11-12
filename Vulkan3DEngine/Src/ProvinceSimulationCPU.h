#pragma once
#include "IProvinceSimulation.h"
#include "ThreadPool.h"
#include "AsyncMemoryOps.h"
#include <vector>
#include <atomic>
#include <mutex>
#include <future>
#include <memory>

/**
 * CPU-based province simulation using ThreadPool and AsyncMemoryOps
 * Uses unified structures from ProvinceSimulationCommon.h
 */
class ProvinceSimulationCPU : public IProvinceSimulation {
public:
    explicit ProvinceSimulationCPU(ThreadPool* threadPool, AsyncMemoryOps* asyncMemOps = nullptr);
    ~ProvinceSimulationCPU() override;

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

    void requestDataRefresh() override {} // No-op for CPU
    bool isDataRefreshComplete() const override { return true; } // Always ready

    const char* getTypeName() const override { return "CPU"; }

    // CPU-specific configuration
    void setThreadCount(size_t threads);
    size_t getThreadCount() const { return threadCount_; }

private:
    ThreadPool* threadPool_;
    AsyncMemoryOps* asyncMemOps_;
    size_t threadCount_;

    SimulationParameters simParams_;
    RandomizationParameters randParams_;

    // Unified buffer structure
    std::unique_ptr<ProvinceDataBuffer> workingBuffer_;
    std::vector<ProvinceData> initialStats_;

    std::atomic<uint32_t> stepCounter_{ 0 };
    std::atomic<uint32_t> pendingSteps_{ 0 };
    std::atomic<bool> computeInProgress_{ false };

    // Async operation tracking
    ShaderLib::AsyncOperationHandle activeOperationHandle_;
    std::vector<std::future<void>> activeFutures_; // Fallback when no AsyncMemoryOps
    mutable std::mutex dataMutex_;

    // Core simulation logic (matches GPU shader)
    void simulateProvinceRange(uint32_t startIdx, uint32_t endIdx);
    void simulateSingleProvince(uint32_t idx);

    // Async execution
    void dispatchCompute();
    void waitForCompletion();
};
