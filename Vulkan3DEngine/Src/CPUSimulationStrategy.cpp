#include "CPUSimulationStrategy.h"
#include <spdlog/spdlog.h>
#include <random>
#include <algorithm>
#include <cmath>
#include <chrono>

CPUSimulationStrategy::CPUSimulationStrategy(ThreadPool* threadPool)
    : threadPool_(threadPool)
    , threadCount_(threadPool ? threadPool->getThreadCount() : 1)
{
}

CPUSimulationStrategy::~CPUSimulationStrategy() {
    shutdown();
}

bool CPUSimulationStrategy::initialize(
    const SimulationParameters& simParams,
    const RandomizationParameters& randParams,
    ProvinceDataBuffer* sharedBuffer
) {
    if (!threadPool_ || !sharedBuffer) {
        SPDLOG_ERROR("CPU: Invalid initialization parameters");
        return false;
    }

    simParams_ = simParams;
    sharedBuffer_ = sharedBuffer;
    tickCounter_ = 0;

    // Generate random initial data
    std::mt19937 gen;
    if (randParams.randomSeed == 0) {
        std::random_device rd;
        gen.seed(rd());
    }
    else {
        gen.seed(randParams.randomSeed);
    }

    std::uniform_real_distribution<float> popDist(
        randParams.minPopulation,
        randParams.maxPopulation
    );
    std::uniform_real_distribution<float> foodProdDist(
        randParams.minFoodProduction,
        randParams.maxFoodProduction
    );

    // Initialize active provinces
    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        sharedBuffer_->provinces[i] = {
            popDist(gen),
            foodProdDist(gen),
            0.0f,  // wealth starts at 0
            randParams.initialFoodStorage
        };
    }

    // Zero unused provinces
    for (uint32_t i = simParams_.numProvinces; i < MAX_PROVINCES; ++i) {
        sharedBuffer_->provinces[i] = { 0.0f, 0.0f, 0.0f, 0.0f };
    }

    SPDLOG_INFO("CPU strategy initialized: {} provinces, {} threads",
        simParams_.numProvinces, threadCount_);

    return true;
}

void CPUSimulationStrategy::shutdown() {
    // Nothing to cleanup - thread pool is not owned
}

void CPUSimulationStrategy::executeSingleStep() {
    if (!threadPool_ || !sharedBuffer_) {
        SPDLOG_ERROR("CPU: Strategy not initialized");
        return;
    }

    // =========================================================================
    // START TIMING
    // =========================================================================
    auto startTime = std::chrono::high_resolution_clock::now();

    // =========================================================================
    // 1. DIVIDE WORK INTO BATCHES
    // =========================================================================
    const size_t numProvinces = simParams_.numProvinces;
    const size_t batchSize = (numProvinces + threadCount_ - 1) / threadCount_;

    std::vector<std::future<void>> futures;
    futures.reserve(threadCount_);

    // =========================================================================
    // 2. SUBMIT BATCHES TO THREAD POOL
    // =========================================================================
    for (size_t start = 0; start < numProvinces; start += batchSize) {
        size_t end = std::min(start + batchSize, numProvinces);

        futures.push_back(threadPool_->enqueue([this, start, end]() {
            for (size_t i = start; i < end; ++i) {
                simulateProvince(static_cast<uint32_t>(i));
            }
            }));
    }

    // =========================================================================
    // 3. WAIT FOR ALL BATCHES (BLOCKING)
    // =========================================================================
    for (auto& future : futures) {
        if (future.valid()) {
            future.get();  // BLOCKS until batch completes
        }
    }

    // =========================================================================
    // STOP TIMING
    // =========================================================================
    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(
        endTime - startTime
    ).count();

    StepTimings timings;
    timings.computeMs = elapsedMs;
    timings.readbackMs = 0.0;  // CPU has no readback
    timings.totalMs = elapsedMs;

    {
        std::lock_guard<std::mutex> lock(timingsMutex_);
        lastStepTimings_ = timings;
    }

    tickCounter_.fetch_add(1, std::memory_order_relaxed);

    SPDLOG_TRACE("CPU: Step {} completed in {:.4f}ms",
        tickCounter_.load(), elapsedMs);
}

void CPUSimulationStrategy::setThreadCount(size_t threads) {
    threadCount_ = std::max(size_t(1), threads);
    SPDLOG_INFO("CPU: Thread count set to {}", threadCount_);
}

// =============================================================================
// SIMULATION LOGIC
// =============================================================================

void CPUSimulationStrategy::simulateProvince(uint32_t idx) {
    if (!sharedBuffer_ || idx >= simParams_.numProvinces) return;

    ProvinceData& province = sharedBuffer_->provinces[idx];

    // Check for extinction
    if (province.population < simParams_.minPopulation) {
        province.population = simParams_.minPopulation;
        province.foodStorage = 0.0f;
        province.wealth = 0.0f;
        return;
    }

    // =========================================================================
    // FOOD PRODUCTION
    // =========================================================================
    float foodProduced = province.foodProductionModifier;
    province.foodStorage += foodProduced;
    province.foodStorage = std::min(province.foodStorage, simParams_.maxFoodStorage);

    // =========================================================================
    // FOOD CONSUMPTION
    // =========================================================================
    float foodNeeded = province.population * simParams_.foodConsumptionPerPop;
    float foodConsumed = std::min(province.foodStorage, foodNeeded);
    province.foodStorage -= foodConsumed;

    float foodRatio = foodConsumed / std::max(foodNeeded, 0.001f);

    // =========================================================================
    // POPULATION DYNAMICS
    // =========================================================================
    float populationChange = 0.0f;

    if (foodRatio >= 1.0f) {
        // Surplus food - population grows with bonus
        float excessFood = foodConsumed - foodNeeded;
        float growthBonus = std::min(excessFood * 0.01f, 0.01f);
        populationChange = province.population * (simParams_.basePopulationGrowth + growthBonus);
    }
    else if (foodRatio >= simParams_.starvationThreshold) {
        // Partial food - reduced growth
        float partialGrowth = (foodRatio - simParams_.starvationThreshold) /
            (1.0f - simParams_.starvationThreshold);
        populationChange = province.population * simParams_.basePopulationGrowth * partialGrowth;
    }
    else {
        // Starvation - population decline
        float starvationSeverity = 1.0f - (foodRatio / simParams_.starvationThreshold);
        populationChange = -province.population * 0.05f * starvationSeverity;
    }

    province.population += populationChange;
    province.population = std::max(province.population, simParams_.minPopulation);

    // =========================================================================
    // WEALTH GENERATION
    // =========================================================================
    if (foodRatio >= simParams_.starvationThreshold) {
        float wealthGenerated = province.population * simParams_.wealthPerPop * foodRatio;
        province.wealth += wealthGenerated;
    }
}
