#pragma once
#include "ISimulationStrategy.h"
#include "ThreadPool.h"
#include <atomic>
#include <vector>
#include <future>

/**
 * CPU Strategy - Synchronous multi-threaded execution
 *
 * Flow per step:
 * 1. Start timer
 * 2. Divide work into batches
 * 3. Submit batches to thread pool
 * 4. Wait for all futures (BLOCKING)
 * 5. Stop timer
 * 6. Increment tick
 *
 * Simple, direct, accurate timing.
 */
class CPUSimulationStrategy : public ISimulationStrategy {
public:
    explicit CPUSimulationStrategy(ThreadPool* threadPool);
    ~CPUSimulationStrategy() override;

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
        return "CPU";
    }

    // Configuration
    void setThreadCount(size_t threads);
    size_t getThreadCount() const { return threadCount_; }

private:
    void simulateProvince(uint32_t idx);

    ThreadPool* threadPool_;
    size_t threadCount_;

    SimulationParameters simParams_;
    ProvinceDataBuffer* sharedBuffer_;

    std::atomic<uint32_t> tickCounter_{ 0 };
    std::atomic<double> lastStepTimeMs_{ 0.0 };
};
