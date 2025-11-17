#include "ProvinceSimulationTest.h"
#include "GPUSimulationStrategy.h"
#include "CPUSimulationStrategy.h"
#include "Engine.h"
#include "AssetSystem.h"
#include <spdlog/spdlog.h>

// =============================================================================
// MOVE SEMANTICS (unchanged)
// =============================================================================

ProvinceSimulationTest::ProvinceSimulationTest(ProvinceSimulationTest&& other) noexcept
    : CppScriptBase(std::move(other))
    , currentMode_(other.currentMode_)
    , strategy_(std::move(other.strategy_))
    , simulationThread_(std::move(other.simulationThread_))
    , running_(other.running_.load())
    , stepsRequested_(other.stepsRequested_.load())
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
    other.running_ = false;
    other.stepsRequested_ = 0;
    other.computeDispatcher_ = nullptr;
    other.materialManager_ = nullptr;
    other.threadPool_ = nullptr;
}

ProvinceSimulationTest& ProvinceSimulationTest::operator=(ProvinceSimulationTest&& other) noexcept
{
    if (this != &other)
    {
        if (running_)
        {
            running_ = false;
            if (simulationThread_.joinable())
                simulationThread_.join();
        }

        CppScriptBase::operator=(std::move(other));
        currentMode_ = other.currentMode_;
        strategy_ = std::move(other.strategy_);
        simulationThread_ = std::move(other.simulationThread_);
        running_ = other.running_.load();
        stepsRequested_ = other.stepsRequested_.load();
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

        other.running_ = false;
        other.stepsRequested_ = 0;
        other.computeDispatcher_ = nullptr;
        other.materialManager_ = nullptr;
        other.threadPool_ = nullptr;
    }
    return *this;
}

// =============================================================================
// LIFECYCLE (unchanged)
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

    computeDispatcher_ = &engine->engineCore().renderer().computeDispatcher();
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
    SPDLOG_INFO("ProvinceSimulationTest destroyed for entity {}", entity.id);

    if (isBenchmarkRunning()) {
        cancelBenchmark();
    }

    destroyStrategy();
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
    std::lock_guard<std::mutex> lock(dataMutex_);
    if (!sharedBuffer_ || index >= simParams_.numProvinces) {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    return sharedBuffer_->provinces[index];
}

ProvinceData ProvinceSimulationTest::getInitialStats(uint32_t index) const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    if (index >= initialStats_.size()) {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
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
// STRATEGY MANAGEMENT (unchanged, skipping for brevity)
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

    running_ = false;
    if (simulationThread_.joinable()) {
        simulationThread_.join();
    }

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
            strategy_->executeSingleStep();
            stepsRequested_.fetch_sub(1, std::memory_order_relaxed);
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

    SimulationStatistics stats = computeCurrentStatistics();
    stats.tickNumber = currentTick;
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
    SimulationStatistics stats{};
    std::lock_guard<std::mutex> lock(dataMutex_);

    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        auto data = sharedBuffer_->provinces[i];
        auto initial = i < initialStats_.size() ? initialStats_[i] : ProvinceData{ 0,0,0,0 };

        stats.totalPopulation += data.population;
        stats.totalWealth += data.wealth;

        if (initial.population > 0.0f) {
            float growth = ((data.population - initial.population) / initial.population) * 100.0f;
            stats.avgGrowth += growth;

            if (growth > 5.0f) stats.growing++;
            else if (growth < -5.0f) stats.declining++;
            else stats.stable++;
        }
    }

    if (simParams_.numProvinces > 0) {
        stats.avgGrowth /= simParams_.numProvinces;
    }

    return stats;
}

// =============================================================================
// BENCHMARK - SIMPLIFIED IMPLEMENTATION
// =============================================================================

// ... (Move semantics, lifecycle, simulation control, strategy management, statistics - unchanged)
// Showing only the BENCHMARK section:

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

    SPDLOG_INFO("Starting benchmark - GPU: {}, CPU: {}, Ticks: {}",
        config.benchmarkGPU, config.benchmarkCPU, config.numTicks);

    // Backup current settings
    originalMode_ = currentMode_;
    originalThreads_ = cpuThreadCount_;
    originalReadbackInterval_ = getGPUReadbackInterval();

    // Store config and clear results
    benchmarkConfig_ = config;
    benchmarkResults_.clear();
    currentPhase_ = BenchmarkPhase::None;

    // Mark as running
    benchmarkRunning_ = true;

    // Start first phase
    startNextPhase();
}

void ProvinceSimulationTest::cancelBenchmark() {
    if (!benchmarkRunning_) return;

    SPDLOG_INFO("Cancelling benchmark");

    benchmarkRunning_ = false;
    currentPhase_ = BenchmarkPhase::None;
    stepsRequested_.store(0, std::memory_order_release);

    // Wait for current steps to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Restore settings
    setMode(originalMode_);
    setCPUThreadCount(originalThreads_);
    setGPUReadbackInterval(originalReadbackInterval_);
    resetSimulation();

    if (benchmarkCallback_) {
        benchmarkCallback_(false, nullptr);
    }
}

void ProvinceSimulationTest::startNextPhase() {
    if (!benchmarkRunning_) return;

    // Determine next phase
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

    // Start the next phase
    currentPhase_ = nextPhase;

    SPDLOG_INFO("Starting benchmark phase: {}",
        currentPhase_ == BenchmarkPhase::GPU ? "GPU" : "CPU");

    // Clear step queue
    stepsRequested_.store(0, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Reset simulation
    resetSimulation();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Configure for this phase
    if (currentPhase_ == BenchmarkPhase::GPU) {
        setMode(SimulationMode::GPU);
        setGPUReadbackInterval(originalReadbackInterval_);
    }
    else {
        setMode(SimulationMode::CPU);
        setCPUThreadCount(benchmarkConfig_.cpuThreads);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Record start tick (for tracking completion)
    phaseStartTick_ = getCurrentTick();

    // Clear accumulated times
    phaseSamples_.clear();

    // Force auto-readback during benchmark
    if (strategy_) {
        strategy_->setAutoReadback(true);
    }

    // Queue the steps
    runMultipleSteps(benchmarkConfig_.numTicks);

    SPDLOG_INFO("Phase started - {} ticks queued, starting from tick {}",
        benchmarkConfig_.numTicks, phaseStartTick_);
}

void ProvinceSimulationTest::updateBenchmark() {
    if (!benchmarkRunning_ || !strategy_) return;
    if (currentPhase_ == BenchmarkPhase::None) return;

    uint32_t currentTick = getCurrentTick();
    uint32_t ticksCompleted = currentTick - phaseStartTick_;
    uint32_t stepsRemaining = stepsRequested_.load(std::memory_order_acquire);

    // Collect timing samples
    if (ticksCompleted > phaseSamples_.size()) {
        StepTimings timings = strategy_->getLastStepTimings();
        if (timings.totalMs > 0.0) {
            phaseSamples_.push_back({
                timings.computeMs,
                timings.readbackMs,
                timings.totalMs
                });
        }
    }

    // Check if phase is complete
    if (ticksCompleted >= benchmarkConfig_.numTicks && stepsRemaining == 0) {
        SPDLOG_INFO("Phase complete: {} ticks done, {} samples collected",
            ticksCompleted, phaseSamples_.size());
        finishCurrentPhase();
    }
}

void ProvinceSimulationTest::finishCurrentPhase() {
    uint32_t actualTicks = getCurrentTick() - phaseStartTick_;

    // Calculate statistics from collected samples
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

    // Create result
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

    benchmarkRunning_ = false;
    currentPhase_ = BenchmarkPhase::None;
    stepsRequested_.store(0, std::memory_order_release);

    // Restore original settings
    setMode(originalMode_);
    setCPUThreadCount(originalThreads_);
    setGPUReadbackInterval(originalReadbackInterval_);
    resetSimulation();

    // Notify completion
    if (benchmarkCallback_) {
        benchmarkCallback_(true, nullptr);
    }
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
