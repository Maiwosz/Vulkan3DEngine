#pragma once
#include "CppScriptBase.h"
#include "ISimulationStrategy.h"
#include "MaterialManager.h"
#include "ComputeDispatcher.h"
#include "ThreadPool.h"
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

struct SimulationStatistics {
    uint32_t tickNumber;
    float totalPopulation;
    float totalWealth;
    float avgGrowth;
    uint32_t growing;
    uint32_t stable;
    uint32_t declining;
    double tickDurationMs;
};

struct BenchmarkResult {
    SimulationMode mode;
    size_t cpuThreads;
    uint32_t numProvinces;
    uint32_t numTicks;

    // Detailed timings
    double totalComputeMs;
    double totalReadbackMs;
    double totalTimeMs;

    double avgComputePerTick;
    double avgReadbackPerTick;
    double avgTimePerTick;

    double minComputePerTick;
    double maxComputePerTick;
    double minReadbackPerTick;
    double maxReadbackPerTick;

    double minTimePerTick;
    double maxTimePerTick;

    double ticksPerSecond;
};

struct BenchmarkConfig {
    bool benchmarkGPU = true;
    bool benchmarkCPU = true;
    uint32_t numTicks = 100;
    size_t cpuThreads = 8;
};

class ProvinceSimulationTest : public CppScriptBase {
public:
    ProvinceSimulationTest() = default;
    ~ProvinceSimulationTest() override = default;

    ProvinceSimulationTest(const ProvinceSimulationTest&) = delete;
    ProvinceSimulationTest& operator=(const ProvinceSimulationTest&) = delete;
    ProvinceSimulationTest(ProvinceSimulationTest&&) noexcept;
    ProvinceSimulationTest& operator=(ProvinceSimulationTest&&) noexcept;

    const char* getScriptName() const override;
    void OnCreate() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;

    // =========================================================================
    // SIMULATION CONTROL
    // =========================================================================

    void setMode(SimulationMode mode);
    SimulationMode getMode() const { return currentMode_; }

    void setCPUThreadCount(size_t threads);
    size_t getCPUThreadCount() const;

    void setGPUReadbackInterval(uint32_t interval);
    uint32_t getGPUReadbackInterval() const;

    void setAutoReadback(bool enabled);
    bool isAutoReadback() const;
    void triggerManualReadback();

    void runSingleStep();
    void runMultipleSteps(uint32_t numSteps);
    void resetSimulation();
    void resetSimulationWithParameters(
        const SimulationParameters& simParams,
        const RandomizationParameters& randParams
    );

    // =========================================================================
    // SIMULATION STATE
    // =========================================================================

    bool isSimulationRunning() const { return simulationThread_.joinable(); }
    bool hasStepsRequested() const {
        return stepsRequested_.load(std::memory_order_relaxed) > 0;
    }

    uint32_t getCurrentTick() const;
    uint32_t getNumProvinces() const { return simParams_.numProvinces; }

    const SimulationParameters& getSimulationParameters() const { return simParams_; }
    const RandomizationParameters& getRandomizationParameters() const { return randParams_; }

    void setSimulationParameters(const SimulationParameters& params);
    void setRandomizationParameters(const RandomizationParameters& params);

    // =========================================================================
    // DATA ACCESS (thread-safe)
    // =========================================================================

    ProvinceData getProvinceData(uint32_t index) const;
    ProvinceData getInitialStats(uint32_t index) const;

    // =========================================================================
    // STATISTICS
    // =========================================================================

    const std::vector<SimulationStatistics>& getStatisticsHistory() const {
        return statisticsHistory_;
    }

    const SimulationStatistics* getLatestStatistics() const {
        return statisticsHistory_.empty() ? nullptr : &statisticsHistory_.back();
    }

    void clearStatisticsHistory() { statisticsHistory_.clear(); }

    struct PerformanceStats {
        uint32_t currentTick;
        StepTimings lastTimings;
    };
    PerformanceStats getPerformanceStats() const;

    // =========================================================================
    // BENCHMARK - SIMPLIFIED
    // =========================================================================

    void startBenchmark(const BenchmarkConfig& config);
    void cancelBenchmark();

    bool isBenchmarkRunning() const { return benchmarkRunning_; }
    float getBenchmarkProgress() const;
    const char* getBenchmarkPhaseDescription() const;

    const std::vector<BenchmarkResult>& getBenchmarkResults() const {
        return benchmarkResults_;
    }
    void clearBenchmarkResults() { benchmarkResults_.clear(); }

    // =========================================================================
    // CALLBACKS
    // =========================================================================

    using TickCallback = std::function<void(uint32_t tick, const SimulationStatistics& stats)>;
    using BenchmarkCallback = std::function<void(bool completed, const BenchmarkResult* result)>;

    void setTickCallback(TickCallback callback) { tickCallback_ = callback; }
    void setBenchmarkCallback(BenchmarkCallback callback) { benchmarkCallback_ = callback; }

private:
    // =========================================================================
    // STRATEGY PATTERN
    // =========================================================================

    SimulationMode currentMode_ = SimulationMode::GPU;
    std::unique_ptr<ISimulationStrategy> strategy_;

    // Strategy thread
    std::thread simulationThread_;
    std::atomic<bool> running_{ false };
    std::atomic<uint32_t> stepsRequested_{ 0 };

    void simulationThreadFunc();
    void createStrategy(SimulationMode mode);
    void destroyStrategy();

    // =========================================================================
    // SHARED DATA
    // =========================================================================

    std::unique_ptr<ProvinceDataBuffer> sharedBuffer_;
    std::vector<ProvinceData> initialStats_;
    mutable std::mutex dataMutex_;

    SimulationParameters simParams_;
    RandomizationParameters randParams_;

    // =========================================================================
    // RESOURCES
    // =========================================================================

    ComputeDispatcher* computeDispatcher_ = nullptr;
    MaterialManager* materialManager_ = nullptr;
    ThreadPool* threadPool_ = nullptr;

    size_t cpuThreadCount_ = 8;
    uint32_t gpuReadbackInterval_ = 1;

    // =========================================================================
    // STATISTICS
    // =========================================================================

    std::vector<SimulationStatistics> statisticsHistory_;
    static constexpr size_t MAX_HISTORY = 1000;
    uint32_t lastProcessedTick_ = 0;

    void updateStatistics();
    SimulationStatistics computeCurrentStatistics() const;

    // =========================================================================
    // BENCHMARK - SIMPLIFIED STATE
    // =========================================================================

    enum class BenchmarkPhase {
        None,
        GPU,
        CPU
    };

    std::atomic<bool> benchmarkRunning_{ false };
    BenchmarkPhase currentPhase_ = BenchmarkPhase::None;
    BenchmarkConfig benchmarkConfig_;
    std::vector<BenchmarkResult> benchmarkResults_;

    struct PhaseSample {
        double computeMs;
        double readbackMs;
        double totalMs;
    };
    std::vector<PhaseSample> phaseSamples_;

    uint32_t phaseStartTick_ = 0;
    std::chrono::steady_clock::time_point phaseStartTime_;

    // Backup settings
    SimulationMode originalMode_;
    size_t originalThreads_ = 0;
    uint32_t originalReadbackInterval_ = 1;

    void updateBenchmark();
    void startNextPhase();
    void finishCurrentPhase();
    void completeBenchmark();

    TickCallback tickCallback_;
    BenchmarkCallback benchmarkCallback_;
};
