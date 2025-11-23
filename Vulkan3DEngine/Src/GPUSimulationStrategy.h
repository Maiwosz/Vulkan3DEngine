#pragma once
#include "ISimulationStrategy.h"
#include "MaterialManager.h"
#include "ComputeDispatcher.h"
#include <atomic>
#include <vector>

// GPU-specific aggregate data structure (matches shader OutputData)
struct GPUAggregateData {
    uint32_t totalPopulation;
    uint32_t totalWealth;
    int32_t avgGrowthScaled;
    uint32_t growing;
    uint32_t stable;
    uint32_t declining;
};

/**
 * GPU Strategy with Dual Aggregation Modes
 *
 * - GPU Mode: Aggregates computed in shader, always fast
 * - CPU Fallback Mode: When readbackFullData_ is enabled, reads all provinces
 *   and computes aggregates on CPU (identical to CPU strategy)
 *
 * Flow per step:
 * 1. Clear output buffer (aggregates)
 * 2. Dispatch compute (calculates simulation + GPU aggregates)
 * 3. Readback GPU aggregates (always, small ~24 bytes)
 * 4. If readbackFullData_: Read all provinces + compute CPU aggregates
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
    void executeSingleStep() override;

    uint32_t getCurrentTick() const override {
        return tickCounter_.load(std::memory_order_relaxed);
    }

    const char* getTypeName() const override {
        return "GPU";
    }

    // Unified aggregate statistics interface
    AggregateStatistics getAggregateStatistics() const override;

    // Configuration
    void setReadbackInterval(uint32_t interval) {
        readbackInterval_ = std::max(1u, interval);
    }

    uint32_t getReadbackInterval() const {
        return readbackInterval_;
    }

    void setReadbackFullData(bool enabled);

    bool isReadbackFullData() const {
        return readbackFullData_;
    }

    StepTimings getLastStepTimings() const override {
        std::lock_guard<std::mutex> lock(timingsMutex_);
        return lastStepTimings_;
    }

    void setAutoReadback(bool enabled) override {
        autoReadback_ = enabled;
    }

    bool isAutoReadback() const override {
        return autoReadback_;
    }

    void manualReadback() override;
    void uploadSingleProvince(uint32_t index, const ProvinceData& data);

    // Individual province access (slow, reads from GPU)
    ProvinceData getProvinceData(uint32_t index);

private:
    void initializeGPUData(const RandomizationParameters& randParams);
    void syncParametersToGPU();
    void readbackFromGPU();
    void readbackGPUAggregates();
    void computeCPUAggregates();

    ComputeDispatcher* dispatcher_;
    MaterialManager* materialManager_;
    MaterialSmartHandle material_;

    SimulationParameters simParams_;
    ProvinceDataBuffer* sharedBuffer_;
    std::vector<ProvinceData> initialStats_;

    std::atomic<uint32_t> tickCounter_{ 0 };
    StepTimings lastStepTimings_;
    mutable std::mutex timingsMutex_;

    bool autoReadback_{ true };
    bool readbackFullData_{ false };

    uint32_t readbackInterval_{ 1 };
    uint32_t ticksSinceReadback_{ 0 };

    // Aggregated statistics
    AggregateStatistics lastAggregates_;
    mutable std::mutex aggregatesMutex_;
};
