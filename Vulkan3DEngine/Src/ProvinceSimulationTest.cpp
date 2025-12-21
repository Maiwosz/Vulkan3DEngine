#include "ProvinceSimulationTest.h"
#include "GPUSimulationStrategy.h"
#include "CPUSimulationStrategy.h"
#include "Engine.h"
#include "AssetSystem.h"
#include <spdlog/spdlog.h>
#include <random>

// =============================================================================
// DESCTRUCTOR, MOVE CONSTRUCTOR, MOVE ASSIGNMENT
// =============================================================================

ProvinceSimulationTest::~ProvinceSimulationTest() {
    // CRITICAL: Stop simulation thread BEFORE destroying anything
    if (running_.load(std::memory_order_acquire)) {
        running_.store(false, std::memory_order_release);
    }

    // Clear any pending work
    stepsRequested_.store(0, std::memory_order_release);

    // Wait for thread to finish
    if (simulationThread_.joinable()) {
        simulationThread_.join();
    }

    // Cancel any running benchmark
    if (benchmarkRunning_.load(std::memory_order_acquire)) {
        benchmarkRunning_.store(false, std::memory_order_release);
    }

    // Now it's safe to destroy the strategy
    if (strategy_) {
        strategy_->shutdown();
        strategy_.reset();
    }

    // Clean up buffer
    sharedBuffer_.reset();

    SPDLOG_INFO("ProvinceSimulationTest destroyed safely");
}

ProvinceSimulationTest::ProvinceSimulationTest(ProvinceSimulationTest&& other) noexcept
    : CppScriptBase(std::move(other))
    , currentMode_(other.currentMode_)
    , strategy_(std::move(other.strategy_))
    // DON'T move thread directly - ensure it's stopped first!
    , running_(false) // Start with stopped state
    , stepsRequested_(0)
    , sharedBuffer_(std::move(other.sharedBuffer_))
    , initialStats_(std::move(other.initialStats_))
    , simParams_(other.simParams_)
    , randParams_(other.randParams_)
    , computeDispatcher_(other.computeDispatcher_)
    , materialManager_(other.materialManager_)
    , threadPool_(other.threadPool_)
    , cpuThreadCount_(other.cpuThreadCount_)
    , gpuReadbackInterval_(other.gpuReadbackInterval_)
    , statisticsHistory_(std::move(other.statisticsHistory_))
    , lastProcessedTick_(other.lastProcessedTick_)
    , benchmarkConfig_(other.benchmarkConfig_)
    , benchmarkResults_(std::move(other.benchmarkResults_))
    , tickCallback_(std::move(other.tickCallback_))
    , benchmarkCallback_(std::move(other.benchmarkCallback_))
{
    // Stop the other's thread before we do anything
    other.running_.store(false, std::memory_order_release);
    other.stepsRequested_.store(0, std::memory_order_release);

    if (other.simulationThread_.joinable()) {
        other.simulationThread_.join();
    }

    // Now move the thread (it's no longer joinable)
    simulationThread_ = std::move(other.simulationThread_);

    // Clear other's pointers
    other.computeDispatcher_ = nullptr;
    other.materialManager_ = nullptr;
    other.threadPool_ = nullptr;
}

ProvinceSimulationTest& ProvinceSimulationTest::operator=(ProvinceSimulationTest&& other) noexcept
{
    if (this != &other)
    {
        // Stop OUR thread first
        if (running_.load(std::memory_order_acquire)) {
            running_.store(false, std::memory_order_release);
            stepsRequested_.store(0, std::memory_order_release);

            if (simulationThread_.joinable()) {
                simulationThread_.join();
            }
        }

        // Stop OTHER's thread
        other.running_.store(false, std::memory_order_release);
        other.stepsRequested_.store(0, std::memory_order_release);

        if (other.simulationThread_.joinable()) {
            other.simulationThread_.join();
        }

        // Now safe to move
        CppScriptBase::operator=(std::move(other));
        currentMode_ = other.currentMode_;
        strategy_ = std::move(other.strategy_);
        simulationThread_ = std::move(other.simulationThread_);
        running_.store(false, std::memory_order_release);
        stepsRequested_.store(0, std::memory_order_release);
        sharedBuffer_ = std::move(other.sharedBuffer_);
        initialStats_ = std::move(other.initialStats_);
        simParams_ = other.simParams_;
        randParams_ = other.randParams_;
        computeDispatcher_ = other.computeDispatcher_;
        materialManager_ = other.materialManager_;
        threadPool_ = other.threadPool_;
        cpuThreadCount_ = other.cpuThreadCount_;
        gpuReadbackInterval_ = other.gpuReadbackInterval_;
        statisticsHistory_ = std::move(other.statisticsHistory_);
        lastProcessedTick_ = other.lastProcessedTick_;
        benchmarkConfig_ = other.benchmarkConfig_;
        benchmarkResults_ = std::move(other.benchmarkResults_);
        tickCallback_ = std::move(other.tickCallback_);
        benchmarkCallback_ = std::move(other.benchmarkCallback_);

        other.computeDispatcher_ = nullptr;
        other.materialManager_ = nullptr;
        other.threadPool_ = nullptr;
    }
    return *this;
}

// =============================================================================
// LIFECYCLE
// =============================================================================

const char* ProvinceSimulationTest::getScriptName() const {
    return "ProvinceSimulationTest";
}

void ProvinceSimulationTest::OnCreate() {
    SPDLOG_INFO("ProvinceSimulationTest created for entity {}", entity.id);

    auto* engine = getEngine();
    if (!engine) {
        SPDLOG_ERROR("Engine not available");
        return;
    }

    computeDispatcher_ = &engine->engineCore().computeDispatcher();
    materialManager_ = &engine->assetSystem().materialManager();
    threadPool_ = &engine->threadPool();

    sharedBuffer_ = std::make_unique<ProvinceDataBuffer>();
    createStrategy(currentMode_);
}

void ProvinceSimulationTest::OnUpdate(float deltaTime) {
    AssetManager& assetManager = getEngine()->assetSystem().assetManager();
    assetManager.ensureReady(AssetHandle(AssetLib::AssetType::Shader, "ProvinceSimulation"));

    updateStatistics();

    // Update benchmark if running
    if (isBenchmarkRunning()) {
        updateBenchmark();
    }
}

void ProvinceSimulationTest::OnDestroy() {
    SPDLOG_INFO("ProvinceSimulationTest OnDestroy for entity {}", entity.id);

    // Cancel benchmark if running
    if (isBenchmarkRunning()) {
        cancelBenchmark();
    }

    // Destroy strategy (this will also stop the thread)
    destroyStrategy();

    // Clean up buffer
    sharedBuffer_.reset();
}

// =============================================================================
// SIMULATION CONTROL (unchanged from before, skipping for brevity)
// =============================================================================

void ProvinceSimulationTest::setMode(SimulationMode mode) {
    if (mode == currentMode_) return;

    SPDLOG_INFO("Switching simulation mode: {} -> {}",
        currentMode_ == SimulationMode::GPU ? "GPU" : "CPU",
        mode == SimulationMode::GPU ? "GPU" : "CPU");

    currentMode_ = mode;
    destroyStrategy();
    createStrategy(mode);
}

void ProvinceSimulationTest::setCPUThreadCount(size_t threads) {
    cpuThreadCount_ = std::max(size_t(1), threads);
    if (currentMode_ == SimulationMode::CPU && strategy_) {
        auto* cpuStrategy = dynamic_cast<CPUSimulationStrategy*>(strategy_.get());
        if (cpuStrategy) cpuStrategy->setThreadCount(cpuThreadCount_);
    }
}

size_t ProvinceSimulationTest::getCPUThreadCount() const {
    if (currentMode_ == SimulationMode::CPU && strategy_) {
        auto* cpuStrategy = dynamic_cast<CPUSimulationStrategy*>(strategy_.get());
        if (cpuStrategy) return cpuStrategy->getThreadCount();
    }
    return cpuThreadCount_;
}

void ProvinceSimulationTest::setGPUReadbackInterval(uint32_t interval) {
    gpuReadbackInterval_ = std::max(1u, interval);
    if (currentMode_ == SimulationMode::GPU && strategy_) {
        auto* gpuStrategy = dynamic_cast<GPUSimulationStrategy*>(strategy_.get());
        if (gpuStrategy) gpuStrategy->setReadbackInterval(gpuReadbackInterval_);
    }
}

uint32_t ProvinceSimulationTest::getGPUReadbackInterval() const {
    if (currentMode_ == SimulationMode::GPU && strategy_) {
        auto* gpuStrategy = dynamic_cast<GPUSimulationStrategy*>(strategy_.get());
        if (gpuStrategy) return gpuStrategy->getReadbackInterval();
    }
    return gpuReadbackInterval_;
}

void ProvinceSimulationTest::setAutoReadback(bool enabled) {
    if (strategy_) {
        strategy_->setAutoReadback(enabled);
    }
}

bool ProvinceSimulationTest::isAutoReadback() const {
    return strategy_ ? strategy_->isAutoReadback() : true;
}

void ProvinceSimulationTest::triggerManualReadback() {
    if (strategy_) {
        strategy_->manualReadback();
    }
}

void ProvinceSimulationTest::setGPUFullDataReadback(bool enabled) {
    if (currentMode_ == SimulationMode::GPU && strategy_) {
        auto* gpuStrategy = dynamic_cast<GPUSimulationStrategy*>(strategy_.get());
        if (gpuStrategy) {
            gpuStrategy->setReadbackFullData(enabled);
        }
    }
}

bool ProvinceSimulationTest::isGPUFullDataReadback() const {
    if (currentMode_ == SimulationMode::GPU && strategy_) {
        auto* gpuStrategy = dynamic_cast<GPUSimulationStrategy*>(strategy_.get());
        if (gpuStrategy) {
            return gpuStrategy->isReadbackFullData();
        }
    }
    return false;
}

void ProvinceSimulationTest::runSingleStep() {
    if (!strategy_) return;
    stepsRequested_.fetch_add(1, std::memory_order_relaxed);
}

void ProvinceSimulationTest::runMultipleSteps(uint32_t numSteps) {
    if (!strategy_ || numSteps == 0) return;
    stepsRequested_.fetch_add(numSteps, std::memory_order_relaxed);
}

void ProvinceSimulationTest::resetSimulation() {
    destroyStrategy();
    createStrategy(currentMode_);
    lastProcessedTick_ = 0;
    statisticsHistory_.clear();
}

void ProvinceSimulationTest::resetSimulationWithParameters(
    const SimulationParameters& simParams,
    const RandomizationParameters& randParams)
{
    simParams_ = simParams;
    randParams_ = randParams;
    resetSimulation();
}

void ProvinceSimulationTest::setSimulationParameters(const SimulationParameters& params) {
    simParams_ = params;
}

void ProvinceSimulationTest::setRandomizationParameters(const RandomizationParameters& params) {
    randParams_ = params;
}

uint32_t ProvinceSimulationTest::getCurrentTick() const {
    return strategy_ ? strategy_->getCurrentTick() : 0;
}

ProvinceData ProvinceSimulationTest::getProvinceData(uint32_t index) const {
    if (index >= simParams_.numProvinces) {
        return { 0, 0.0f, 0, 0 };
    }

    if (currentMode_ == SimulationMode::GPU && strategy_) {
        auto* gpuStrategy = dynamic_cast<GPUSimulationStrategy*>(strategy_.get());
        if (gpuStrategy) {
            return gpuStrategy->getProvinceData(index);
        }
    }

    // CPU mode: direct access to shared buffer
    std::lock_guard<std::mutex> lock(dataMutex_);
    if (!sharedBuffer_) {
        return { 0, 0.0f, 0, 0 };
    }
    return sharedBuffer_->provinces[index];
}

ProvinceData ProvinceSimulationTest::getInitialStats(uint32_t index) const {
    if (!strategy_) {
        return { 0, 0.0f, 0, 0 };
    }

    // Both GPU and CPU strategies now store initial stats internally
    // For now, we can still use the local copy
    std::lock_guard<std::mutex> lock(dataMutex_);
    if (index >= initialStats_.size()) {
        return { 0, 0.0f, 0, 0 };
    }
    return initialStats_[index];
}

void ProvinceSimulationTest::setProvinceData(uint32_t index, const ProvinceData& data) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    if (!sharedBuffer_ || index >= simParams_.numProvinces) {
        SPDLOG_WARN("Cannot set province data: invalid index {}", index);
        return;
    }

    sharedBuffer_->provinces[index] = data;

    // If GPU mode, need to sync to GPU
    if (strategy_ && currentMode_ == SimulationMode::GPU) {
        auto* gpuStrategy = dynamic_cast<GPUSimulationStrategy*>(strategy_.get());
        if (gpuStrategy) {
            gpuStrategy->uploadSingleProvince(index, data);
        }
    }
}

ProvinceSimulationTest::PerformanceStats ProvinceSimulationTest::getPerformanceStats() const {
    PerformanceStats stats;
    stats.currentTick = getCurrentTick();
    stats.lastTimings = strategy_ ? strategy_->getLastStepTimings() : StepTimings{};
    return stats;
}

// =============================================================================
// STRATEGY MANAGEMENT
// =============================================================================

void ProvinceSimulationTest::createStrategy(SimulationMode mode) {
    if (strategy_) {
        SPDLOG_WARN("Strategy already exists");
        return;
    }

    switch (mode) {
    case SimulationMode::GPU: {
        if (!computeDispatcher_ || !materialManager_) {
            SPDLOG_ERROR("GPU resources not available");
            return;
        }
        auto gpuStrategy = std::make_unique<GPUSimulationStrategy>(
            computeDispatcher_, materialManager_
        );
        gpuStrategy->setReadbackInterval(gpuReadbackInterval_);
        strategy_ = std::move(gpuStrategy);
        break;
    }
    case SimulationMode::CPU: {
        if (!threadPool_) {
            SPDLOG_ERROR("ThreadPool not available");
            return;
        }
        auto cpuStrategy = std::make_unique<CPUSimulationStrategy>(threadPool_);
        cpuStrategy->setThreadCount(cpuThreadCount_);
        strategy_ = std::move(cpuStrategy);
        break;
    }
    }

    if (!strategy_->initialize(simParams_, randParams_, sharedBuffer_.get())) {
        SPDLOG_ERROR("Failed to initialize strategy");
        strategy_.reset();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        initialStats_.resize(simParams_.numProvinces);
        for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
            initialStats_[i] = sharedBuffer_->provinces[i];
        }
    }

    running_ = true;
    simulationThread_ = std::thread(&ProvinceSimulationTest::simulationThreadFunc, this);

    SPDLOG_INFO("Strategy created and running: {}", strategy_->getTypeName());
}

void ProvinceSimulationTest::destroyStrategy() {
    if (!strategy_) return;

    // Stop the simulation thread FIRST
    running_.store(false, std::memory_order_release);
    stepsRequested_.store(0, std::memory_order_release);

    // Wait for thread to finish
    if (simulationThread_.joinable()) {
        SPDLOG_DEBUG("Waiting for simulation thread to finish...");
        simulationThread_.join();
        SPDLOG_DEBUG("Simulation thread joined successfully");
    }

    // Now safe to shutdown strategy
    strategy_->shutdown();
    strategy_.reset();

    SPDLOG_INFO("Strategy destroyed");
}

void ProvinceSimulationTest::simulationThreadFunc() {
    SPDLOG_INFO("Simulation thread started");

    while (running_.load(std::memory_order_relaxed)) {
        if (!strategy_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        uint32_t requested = stepsRequested_.load(std::memory_order_relaxed);
        if (requested > 0) {
            // Execute step (BLOCKING - czeka aż się skończy)
            strategy_->executeSingleStep();
            stepsRequested_.fetch_sub(1, std::memory_order_relaxed);

            // Jeśli benchmark działa, sprawdź czy trzeba zrobić snapshot
            if (benchmarkRunning_ && benchmarkConfig_.testConvergence) {
                uint32_t currentTick = strategy_->getCurrentTick();
                uint32_t ticksCompleted = currentTick - phaseStartTick_;
                SimulationMode mode = currentMode_;

                // Pierwszy tick?
                if (ticksCompleted == 1) {
                    if (mode == SimulationMode::CPU && !cpuFirstTick_) {
                        captureConvergenceSnapshot(mode, currentTick);
                        SPDLOG_INFO("✓ Captured CPU FIRST tick (tick={})", currentTick);
                    }
                    else if (mode == SimulationMode::GPU && !gpuFirstTick_) {
                        captureConvergenceSnapshot(mode, currentTick);
                        SPDLOG_INFO("✓ Captured GPU FIRST tick (tick={})", currentTick);
                    }
                }
                // Ostatni tick?
                else if (ticksCompleted == benchmarkConfig_.numTicks) {
                    if (mode == SimulationMode::CPU && !cpuLastTick_) {
                        captureConvergenceSnapshot(mode, currentTick);
                        SPDLOG_INFO("✓ Captured CPU LAST tick (tick={})", currentTick);
                    }
                    else if (mode == SimulationMode::GPU && !gpuLastTick_) {
                        captureConvergenceSnapshot(mode, currentTick);
                        SPDLOG_INFO("✓ Captured GPU LAST tick (tick={})", currentTick);
                    }
                }
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    SPDLOG_INFO("Simulation thread stopped");
}

// =============================================================================
// STATISTICS
// =============================================================================

void ProvinceSimulationTest::updateStatistics() {
    if (!strategy_) return;

    uint32_t currentTick = strategy_->getCurrentTick();
    if (currentTick <= lastProcessedTick_) return;

    lastProcessedTick_ = currentTick;

    // Get aggregates directly from strategy (unified interface)
    AggregateStatistics aggregates = strategy_->getAggregateStatistics();

    // Convert to SimulationStatistics format
    SimulationStatistics stats{};
    stats.tickNumber = currentTick;
    stats.totalPopulation = static_cast<float>(aggregates.totalPopulation);
    stats.totalWealth = static_cast<float>(aggregates.totalWealth);
    stats.avgGrowth = aggregates.avgGrowth;
    stats.growing = aggregates.growing;
    stats.stable = aggregates.stable;
    stats.declining = aggregates.declining;
    stats.tickDurationMs = strategy_->getLastStepTimings().totalMs;

    statisticsHistory_.push_back(stats);

    if (statisticsHistory_.size() > MAX_HISTORY) {
        statisticsHistory_.erase(statisticsHistory_.begin());
    }

    if (tickCallback_) {
        tickCallback_(currentTick, stats);
    }
}

SimulationStatistics ProvinceSimulationTest::computeCurrentStatistics() const {
    if (!strategy_) {
        return SimulationStatistics{};
    }

    // Get aggregates directly from strategy
    AggregateStatistics aggregates = strategy_->getAggregateStatistics();

    // Convert to SimulationStatistics format
    SimulationStatistics stats{};
    stats.totalPopulation = static_cast<float>(aggregates.totalPopulation);
    stats.totalWealth = static_cast<float>(aggregates.totalWealth);
    stats.avgGrowth = aggregates.avgGrowth;
    stats.growing = aggregates.growing;
    stats.stable = aggregates.stable;
    stats.declining = aggregates.declining;

    return stats;
}

// =============================================================================
// BENCHMARK - SIMPLIFIED IMPLEMENTATION
// =============================================================================

void ProvinceSimulationTest::startBenchmark(const BenchmarkConfig& config) {
    if (benchmarkRunning_) {
        SPDLOG_WARN("Benchmark already running");
        return;
    }

    if (!config.benchmarkGPU && !config.benchmarkCPU) {
        SPDLOG_WARN("No benchmark modes selected");
        return;
    }

    // ✨ NOWE: Wygeneruj deterministyczny seed jeśli nie podano
    benchmarkConfig_ = config;
    if (benchmarkConfig_.deterministicSeed == 0) {
        std::random_device rd;
        benchmarkConfig_.deterministicSeed = rd();
        SPDLOG_INFO("Generated deterministic seed for benchmark: {}",
            benchmarkConfig_.deterministicSeed);
    }
    else {
        SPDLOG_INFO("Using provided deterministic seed: {}",
            benchmarkConfig_.deterministicSeed);
    }

    SPDLOG_INFO("Starting benchmark - GPU: {}, CPU: {}, Ticks: {}, Convergence: {}, Seed: {}",
        config.benchmarkGPU, config.benchmarkCPU, config.numTicks,
        config.testConvergence, benchmarkConfig_.deterministicSeed);

    // Backup current settings
    originalMode_ = currentMode_;
    originalThreads_ = cpuThreadCount_;
    originalReadbackInterval_ = getGPUReadbackInterval();
    originalReadbackFullData_ = isGPUFullDataReadback();

    // ✨ NOWE: Backup aktualnych parametrów randomizacji
    originalRandParams_ = randParams_;

    // Store config and clear results
    benchmarkResults_.clear();
    currentPhase_ = BenchmarkPhase::None;

    // Clear snapshots
    cpuFirstTick_.reset();
    cpuLastTick_.reset();
    gpuFirstTick_.reset();
    gpuLastTick_.reset();
    convergenceComparison_.reset();

    benchmarkRunning_ = true;
    startNextPhase();
}

void ProvinceSimulationTest::cancelBenchmark() {
    if (!benchmarkRunning_) return;

    SPDLOG_INFO("Cancelling benchmark");

    benchmarkRunning_ = false;
    currentPhase_ = BenchmarkPhase::None;
    stepsRequested_.store(0, std::memory_order_release);

    // NOWE: Wyczyść snapshoty
    cpuFirstTick_.reset();
    cpuLastTick_.reset();
    gpuFirstTick_.reset();
    gpuLastTick_.reset();

    // Wait for current steps to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Restore settings
    setMode(originalMode_);
    setCPUThreadCount(originalThreads_);
    setGPUReadbackInterval(originalReadbackInterval_);
    setGPUFullDataReadback(originalReadbackFullData_);
    resetSimulationWithParameters(simParams_, originalRandParams_);

    if (benchmarkCallback_) {
        benchmarkCallback_(false, nullptr);
    }
}

void ProvinceSimulationTest::startNextPhase() {
    if (!benchmarkRunning_) return;

    BenchmarkPhase nextPhase = BenchmarkPhase::None;

    if (currentPhase_ == BenchmarkPhase::None) {
        if (benchmarkConfig_.benchmarkGPU) {
            nextPhase = BenchmarkPhase::GPU;
        }
        else if (benchmarkConfig_.benchmarkCPU) {
            nextPhase = BenchmarkPhase::CPU;
        }
    }
    else if (currentPhase_ == BenchmarkPhase::GPU) {
        if (benchmarkConfig_.benchmarkCPU) {
            nextPhase = BenchmarkPhase::CPU;
        }
    }

    if (nextPhase == BenchmarkPhase::None) {
        completeBenchmark();
        return;
    }

    currentPhase_ = nextPhase;

    SPDLOG_INFO("========================================");
    SPDLOG_INFO("Starting benchmark phase: {}",
        currentPhase_ == BenchmarkPhase::GPU ? "GPU" : "CPU");
    SPDLOG_INFO("========================================");

    // Clear queue and wait
    stepsRequested_.store(0, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // ✨ NOWE: Wymuś deterministyczny seed
    RandomizationParameters benchRandParams = originalRandParams_;
    benchRandParams.randomSeed = benchmarkConfig_.deterministicSeed;

    SPDLOG_INFO("Resetting simulation with deterministic seed: {}",
        benchRandParams.randomSeed);

    // Reset z wymuszonym seedem
    resetSimulationWithParameters(simParams_, benchRandParams);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Configure mode
    if (currentPhase_ == BenchmarkPhase::GPU) {
        setMode(SimulationMode::GPU);
        setGPUReadbackInterval(1);
        setGPUFullDataReadback(originalReadbackFullData_);
    }
    else {
        setMode(SimulationMode::CPU);
        setCPUThreadCount(benchmarkConfig_.cpuThreads);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Record starting tick
    phaseStartTick_ = getCurrentTick();
    phaseSamples_.clear();

    // Enable auto-readback
    if (strategy_) {
        strategy_->setAutoReadback(true);
    }

    // Queue work
    runMultipleSteps(benchmarkConfig_.numTicks);

    SPDLOG_INFO("Phase started - {} ticks queued from tick {} with seed {}",
        benchmarkConfig_.numTicks, phaseStartTick_, benchRandParams.randomSeed);
}

void ProvinceSimulationTest::updateBenchmark() {
    if (!benchmarkRunning_ || !strategy_) return;
    if (currentPhase_ == BenchmarkPhase::None) return;

    uint32_t currentTick = getCurrentTick();
    uint32_t ticksCompleted = currentTick - phaseStartTick_;
    uint32_t stepsRemaining = stepsRequested_.load(std::memory_order_acquire);

    // Zbieraj timings (snapshoty są robione w wątku symulacji)
    if (ticksCompleted > phaseSamples_.size() && ticksCompleted > 0) {
        StepTimings timings = strategy_->getLastStepTimings();
        if (timings.totalMs > 0.0) {
            phaseSamples_.push_back({
                timings.computeMs,
                timings.readbackMs,
                timings.totalMs
                });
        }
    }

    // Sprawdź czy faza się skończyła
    if (ticksCompleted >= benchmarkConfig_.numTicks && stepsRemaining == 0) {
        SPDLOG_INFO("Phase complete: {} ticks done, {} samples collected",
            ticksCompleted, phaseSamples_.size());
        finishCurrentPhase();
    }
}

void ProvinceSimulationTest::finishCurrentPhase() {
    uint32_t actualTicks = getCurrentTick() - phaseStartTick_;

    // Oblicz statystyki z zebranych sampli
    double totalComputeMs = 0.0;
    double totalReadbackMs = 0.0;
    double totalTimeMs = 0.0;

    double minCompute = std::numeric_limits<double>::max();
    double maxCompute = 0.0;
    double minReadback = std::numeric_limits<double>::max();
    double maxReadback = 0.0;
    double minTotal = std::numeric_limits<double>::max();
    double maxTotal = 0.0;

    for (const auto& sample : phaseSamples_) {
        totalComputeMs += sample.computeMs;
        totalReadbackMs += sample.readbackMs;
        totalTimeMs += sample.totalMs;

        minCompute = std::min(minCompute, sample.computeMs);
        maxCompute = std::max(maxCompute, sample.computeMs);
        minReadback = std::min(minReadback, sample.readbackMs);
        maxReadback = std::max(maxReadback, sample.readbackMs);
        minTotal = std::min(minTotal, sample.totalMs);
        maxTotal = std::max(maxTotal, sample.totalMs);
    }

    // Utwórz wynik
    BenchmarkResult result;
    result.mode = (currentPhase_ == BenchmarkPhase::GPU) ? SimulationMode::GPU : SimulationMode::CPU;
    result.cpuThreads = cpuThreadCount_;
    result.numProvinces = simParams_.numProvinces;
    result.numTicks = actualTicks;

    result.totalComputeMs = totalComputeMs;
    result.totalReadbackMs = totalReadbackMs;
    result.totalTimeMs = totalTimeMs;

    result.avgComputePerTick = totalComputeMs / actualTicks;
    result.avgReadbackPerTick = totalReadbackMs / actualTicks;
    result.avgTimePerTick = totalTimeMs / actualTicks;

    result.minComputePerTick = minCompute;
    result.maxComputePerTick = maxCompute;
    result.minReadbackPerTick = minReadback;
    result.maxReadbackPerTick = maxReadback;
    result.minTimePerTick = minTotal;
    result.maxTimePerTick = maxTotal;

    result.ticksPerSecond = (actualTicks * 1000.0) / totalTimeMs;

    benchmarkResults_.push_back(result);

    SPDLOG_INFO("Phase result - Mode: {}, Compute: {:.2f}ms, Readback: {:.2f}ms, Total: {:.2f}ms",
        result.mode == SimulationMode::GPU ? "GPU" : "CPU",
        result.totalComputeMs, result.totalReadbackMs, result.totalTimeMs);

    if (benchmarkCallback_) {
        benchmarkCallback_(false, &result);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    startNextPhase();
}

void ProvinceSimulationTest::completeBenchmark() {
    SPDLOG_INFO("Benchmark complete - all phases finished");

    // NOWE: Oblicz porównanie zbieżności jeśli dostępne
    if (benchmarkConfig_.testConvergence) {
        computeConvergenceComparison();
    }

    benchmarkRunning_ = false;
    currentPhase_ = BenchmarkPhase::None;
    stepsRequested_.store(0, std::memory_order_release);

    // Restore original settings
    setMode(originalMode_);
    setCPUThreadCount(originalThreads_);
    setGPUReadbackInterval(originalReadbackInterval_);
    setGPUFullDataReadback(originalReadbackFullData_);
    resetSimulationWithParameters(simParams_, originalRandParams_);

    // Notify completion
    if (benchmarkCallback_) {
        benchmarkCallback_(true, nullptr);
    }
}

// =============================================================================
// CONVERGENCE TESTING IMPLEMENTATION
// =============================================================================

void ProvinceSimulationTest::captureConvergenceSnapshot(SimulationMode mode, uint32_t tick) {
    if (!strategy_) {
        SPDLOG_ERROR("Cannot capture snapshot - no strategy");
        return;
    }

    uint32_t provinceIdx = benchmarkConfig_.convergenceProvinceIndex;
    if (provinceIdx == 0) {
        provinceIdx = simParams_.numProvinces / 2;
    }

    // Dla GPU - dane są już dostępne po executeSingleStep (które jest blocking)
    // Dla CPU - dane są zawsze dostępne w sharedBuffer_

    ProvinceData provinceData = getProvinceData(provinceIdx);
    AggregateStatistics aggregates = strategy_->getAggregateStatistics();

    ConvergenceSnapshot snapshot;
    snapshot.province = ProvinceSnapshot{
        tick,
        provinceIdx,
        provinceData
    };
    snapshot.aggregate = AggregateSnapshot{
        tick,
        aggregates.totalPopulation,
        aggregates.totalWealth,
        aggregates.avgGrowth,
        aggregates.growing,
        aggregates.stable,
        aggregates.declining
    };

    // Zapisz snapshot
    if (mode == SimulationMode::CPU) {
        if (!cpuFirstTick_) {
            cpuFirstTick_ = snapshot;
        }
        else {
            cpuLastTick_ = snapshot;
        }
    }
    else {
        if (!gpuFirstTick_) {
            gpuFirstTick_ = snapshot;
        }
        else {
            gpuLastTick_ = snapshot;
        }
    }

    SPDLOG_DEBUG("Snapshot: mode={}, tick={}, province={}, pop={}, wealth={}",
        mode == SimulationMode::GPU ? "GPU" : "CPU",
        tick, provinceIdx, provinceData.population, provinceData.wealth);
}

ConvergenceComparison::TickComparison::ProvinceErrors
ProvinceSimulationTest::computeProvinceErrors(
    const ProvinceSnapshot& cpu,
    const ProvinceSnapshot& gpu)
{
    ConvergenceComparison::TickComparison::ProvinceErrors errors;

    // Population errors
    errors.populationAbsError = std::abs(
        static_cast<int64_t>(cpu.data.population) -
        static_cast<int64_t>(gpu.data.population)
    );

    float avgPop = (static_cast<float>(cpu.data.population) + static_cast<float>(gpu.data.population)) / 2.0f;
    errors.populationRelError = avgPop > 0 ?
        (errors.populationAbsError / avgPop) : 0.0f;

    // Wealth errors
    errors.wealthAbsError = std::abs(
        static_cast<int64_t>(cpu.data.wealth) -
        static_cast<int64_t>(gpu.data.wealth)
    );
    float avgWealth = (static_cast<float>(cpu.data.wealth) + static_cast<float>(gpu.data.wealth)) / 2.0f;
    errors.wealthRelError = avgWealth > 0 ?
        (errors.wealthAbsError / avgWealth) : 0.0f;

    // Growth errors - USUNĄĆ, bo ProvinceData nie ma pola growth
    errors.growthAbsError = 0.0f;
    errors.growthRelError = 0.0f;

    return errors;
}

ConvergenceComparison::TickComparison::AggregateErrors
ProvinceSimulationTest::computeAggregateErrors(
    const AggregateSnapshot& cpu,
    const AggregateSnapshot& gpu)
{
    ConvergenceComparison::TickComparison::AggregateErrors errors;

    // Population errors
    errors.populationAbsError = std::abs(
        static_cast<int64_t>(cpu.totalPopulation) -
        static_cast<int64_t>(gpu.totalPopulation)
    );
    double avgPop = (static_cast<double>(cpu.totalPopulation) + static_cast<double>(gpu.totalPopulation)) / 2.0;
    errors.populationRelError = avgPop > 0 ?
        (errors.populationAbsError / avgPop) : 0.0;

    // Wealth errors
    errors.wealthAbsError = std::abs(
        static_cast<int64_t>(cpu.totalWealth) -
        static_cast<int64_t>(gpu.totalWealth)
    );
    double avgWealth = (static_cast<double>(cpu.totalWealth) + static_cast<double>(gpu.totalWealth)) / 2.0;
    errors.wealthRelError = avgWealth > 0 ?
        (errors.wealthAbsError / avgWealth) : 0.0;

    // Growth errors
    errors.growthAbsError = std::fabs(cpu.avgGrowth - gpu.avgGrowth);
    float avgGrowth = std::fabs((cpu.avgGrowth + gpu.avgGrowth) / 2.0f);
    errors.growthRelError = avgGrowth > 0.0001f ?
        (errors.growthAbsError / avgGrowth) : 0.0f;

    return errors;
}

void ProvinceSimulationTest::computeConvergenceComparison() {
    if (!cpuFirstTick_ || !cpuLastTick_ || !gpuFirstTick_ || !gpuLastTick_) {
        SPDLOG_WARN("Cannot compute convergence - missing snapshots");
        return;
    }

    ConvergenceComparison comparison;
    comparison.comparedProvinceIndex = cpuFirstTick_->province.provinceIndex;

    // First tick comparison
    comparison.firstTick.cpuProvince = cpuFirstTick_->province;
    comparison.firstTick.gpuProvince = gpuFirstTick_->province;
    comparison.firstTick.cpuAggregate = cpuFirstTick_->aggregate;
    comparison.firstTick.gpuAggregate = gpuFirstTick_->aggregate;
    comparison.firstTick.provinceErrors = computeProvinceErrors(
        cpuFirstTick_->province, gpuFirstTick_->province
    );
    comparison.firstTick.aggregateErrors = computeAggregateErrors(
        cpuFirstTick_->aggregate, gpuFirstTick_->aggregate
    );

    // Last tick comparison
    comparison.lastTick.cpuProvince = cpuLastTick_->province;
    comparison.lastTick.gpuProvince = gpuLastTick_->province;
    comparison.lastTick.cpuAggregate = cpuLastTick_->aggregate;
    comparison.lastTick.gpuAggregate = gpuLastTick_->aggregate;
    comparison.lastTick.provinceErrors = computeProvinceErrors(
        cpuLastTick_->province, gpuLastTick_->province
    );
    comparison.lastTick.aggregateErrors = computeAggregateErrors(
        cpuLastTick_->aggregate, gpuLastTick_->aggregate
    );

    // Overall assessment - znajdź największy względny błąd
    double maxError = 0.0;

    auto checkError = [&maxError](double err) {
        maxError = std::max(maxError, err);
        };

    // First tick errors
    checkError(comparison.firstTick.provinceErrors.populationRelError);
    checkError(comparison.firstTick.provinceErrors.wealthRelError);
    checkError(comparison.firstTick.aggregateErrors.populationRelError);
    checkError(comparison.firstTick.aggregateErrors.wealthRelError);
    checkError(comparison.firstTick.aggregateErrors.growthRelError);

    // Last tick errors
    checkError(comparison.lastTick.provinceErrors.populationRelError);
    checkError(comparison.lastTick.provinceErrors.wealthRelError);
    checkError(comparison.lastTick.aggregateErrors.populationRelError);
    checkError(comparison.lastTick.aggregateErrors.wealthRelError);
    checkError(comparison.lastTick.aggregateErrors.growthRelError);

    comparison.maxRelativeError = maxError;

    // Uznajemy za znaczącą rozbieżność jeśli błąd > 0.1% (0.001)
    comparison.hasSignificantDivergence = maxError > 0.001;

    convergenceComparison_ = comparison;

    SPDLOG_INFO("Convergence comparison complete - Max relative error: {:.6f}% ({})",
        maxError * 100.0,
        comparison.hasSignificantDivergence ? "SIGNIFICANT" : "OK");
}

float ProvinceSimulationTest::getBenchmarkProgress() const {
    if (!benchmarkRunning_) return 0.0f;
    if (currentPhase_ == BenchmarkPhase::None) return 0.0f;

    uint32_t currentTick = getCurrentTick();
    uint32_t ticksCompleted = currentTick - phaseStartTick_;

    return std::min(1.0f, static_cast<float>(ticksCompleted) / benchmarkConfig_.numTicks);
}

const char* ProvinceSimulationTest::getBenchmarkPhaseDescription() const {
    if (!benchmarkRunning_) return "Not running";

    switch (currentPhase_) {
    case BenchmarkPhase::GPU:
        return "GPU Phase";
    case BenchmarkPhase::CPU:
        return "CPU Phase";
    case BenchmarkPhase::None:
    default:
        return "Initializing...";
    }
}
