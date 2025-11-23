#pragma once
#include "ISimulationStrategy.h"
#include "ThreadPool.h"
#include <atomic>
#include <vector>
#include <future>
#include <mutex>

/**
 * CPU Strategy - With Built-in Aggregation
 *
 * Computes aggregate statistics during simulation by maintaining
 * per-thread accumulators and combining them after each step.
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
    void executeSingleStep() override;

    uint32_t getCurrentTick() const override {
        return tickCounter_.load(std::memory_order_relaxed);
    }

    const char* getTypeName() const override {
        return "CPU";
    }

    // Aggregate statistics
    AggregateStatistics getAggregateStatistics() const override;

    // Configuration
    void setThreadCount(size_t threads);
    size_t getThreadCount() const { return threadCount_; }

    StepTimings getLastStepTimings() const override {
        std::lock_guard<std::mutex> lock(timingsMutex_);
        return lastStepTimings_;
    }

    void setAutoReadback(bool enabled) override {
        // CPU doesn't need readback, but implement for interface consistency
    }

    bool isAutoReadback() const override {
        return true;  // Always "auto" for CPU
    }

    void manualReadback() override {
        // No-op for CPU
    }

private:
    struct ThreadLocalAggregates {
        uint32_t totalPopulation = 0;
        uint32_t totalWealth = 0;
        int64_t sumGrowthScaled = 0;
        uint32_t growing = 0;
        uint32_t stable = 0;
        uint32_t declining = 0;
    };

    void simulateProvince(uint32_t idx, ThreadLocalAggregates& aggregates);
    void computeAggregates();

    ThreadPool* threadPool_;
    size_t threadCount_;

    SimulationParameters simParams_;
    ProvinceDataBuffer* sharedBuffer_;
    std::vector<ProvinceData> initialStats_;

    std::atomic<uint32_t> tickCounter_{ 0 };
    StepTimings lastStepTimings_;
    mutable std::mutex timingsMutex_;

    // Aggregate statistics
    AggregateStatistics lastAggregates_;
    mutable std::mutex aggregatesMutex_;
};
