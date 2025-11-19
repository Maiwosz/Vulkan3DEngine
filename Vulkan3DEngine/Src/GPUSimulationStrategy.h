#pragma once
#include "ISimulationStrategy.h"
#include "MaterialManager.h"
#include "ComputeDispatcher.h"
#include <atomic>
#include <vector>

// GPU aggregated statistics structure (matches shader OutputData)
struct AggregateData {
    uint32_t totalPopulation;   // Changed to uint32_t
    uint32_t totalWealth;       // Changed to uint32_t
    float avgGrowth;
    uint32_t growing;
    uint32_t stable;
    uint32_t declining;
};

struct InitialProvinceStats {
    uint32_t population;        // Changed to uint32_t
};

/**
 * GPU Strategy with Aggregation
 *
 * Flow per step:
 * 1. Clear output buffer (aggregates)
 * 2. Dispatch compute (calculates simulation + aggregates)
 * 3. Readback aggregates (always, small ~24 bytes)
 * 4. Readback full data (optional, large, only when needed)
 *
 * This allows UI to always have fresh statistics without reading
 * all province data every tick.
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

    // Configuration
    void setReadbackInterval(uint32_t interval) {
        readbackInterval_ = std::max(1u, interval);
    }

    uint32_t getReadbackInterval() const {
        return readbackInterval_;
    }

    void setReadbackFullData(bool enabled) {
        readbackFullData_ = enabled;
    }

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

    // Aggregate statistics access (fast, always current)
    AggregateData getAggregateStats() const;

    // Individual province access (slow, reads from GPU)
    ProvinceData getProvinceData(uint32_t index);
    InitialProvinceStats getInitialStats(uint32_t index) const;

private:
    void initializeGPUData(const RandomizationParameters& randParams);
    void syncParametersToGPU();
    void readbackFromGPU();
    void readbackAggregates();

    ComputeDispatcher* dispatcher_;
    MaterialManager* materialManager_;
    MaterialSmartHandle material_;

    SimulationParameters simParams_;
    ProvinceDataBuffer* sharedBuffer_;

    std::atomic<uint32_t> tickCounter_{ 0 };
    StepTimings lastStepTimings_;
    mutable std::mutex timingsMutex_;

    bool autoReadback_{ true };
    bool readbackFullData_{ false };  // Only read full data when explicitly needed

    uint32_t readbackInterval_{ 1 };
    uint32_t ticksSinceReadback_{ 0 };

    // Aggregated statistics (small, fast to read)
    AggregateData lastAggregateStats_;
    mutable std::mutex aggregateMutex_;
};
