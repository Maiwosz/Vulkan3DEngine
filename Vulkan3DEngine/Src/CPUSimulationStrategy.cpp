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

    std::mt19937 gen;
    if (randParams.randomSeed == 0) {
        std::random_device rd;
        gen.seed(rd());
    }
    else {
        gen.seed(randParams.randomSeed);
    }

    std::uniform_int_distribution<uint32_t> popDist(
        randParams.minPopulation,
        randParams.maxPopulation
    );
    std::uniform_real_distribution<float> foodProdDist(
        randParams.minFoodProduction,
        randParams.maxFoodProduction
    );

    // Initialize provinces
    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        sharedBuffer_->provinces[i] = {
            popDist(gen),
            foodProdDist(gen),
            0,
            randParams.initialFoodStorage
        };
    }

    for (uint32_t i = simParams_.numProvinces; i < MAX_PROVINCES; ++i) {
        sharedBuffer_->provinces[i] = { 0, 0.0f, 0, 0 };
    }

    // Store initial stats for growth calculation
    initialStats_.resize(simParams_.numProvinces);
    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        initialStats_[i] = sharedBuffer_->provinces[i];
    }

    // Initialize aggregates
    computeAggregates();

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

    auto startTime = std::chrono::high_resolution_clock::now();

    // =========================================================================
    // 1. DIVIDE WORK INTO BATCHES
    // =========================================================================
    const size_t numProvinces = simParams_.numProvinces;
    const size_t batchSize = (numProvinces + threadCount_ - 1) / threadCount_;

    std::vector<std::future<ThreadLocalAggregates>> futures;
    futures.reserve(threadCount_);

    // =========================================================================
    // 2. SUBMIT BATCHES TO THREAD POOL (with aggregation)
    // =========================================================================
    for (size_t start = 0; start < numProvinces; start += batchSize) {
        size_t end = std::min(start + batchSize, numProvinces);

        futures.push_back(threadPool_->enqueue([this, start, end]() {
            ThreadLocalAggregates aggregates;
            for (size_t i = start; i < end; ++i) {
                simulateProvince(static_cast<uint32_t>(i), aggregates);
            }
            return aggregates;
            }));
    }

    // =========================================================================
    // 3. WAIT AND COMBINE AGGREGATES
    // =========================================================================
    AggregateStatistics combined{};

    for (auto& future : futures) {
        if (future.valid()) {
            ThreadLocalAggregates threadAgg = future.get();

            combined.totalPopulation += threadAgg.totalPopulation;
            combined.totalWealth += threadAgg.totalWealth;
            combined.avgGrowth += threadAgg.sumGrowthScaled;  // ZMIANA: sumuj skalowane int
            combined.growing += threadAgg.growing;
            combined.stable += threadAgg.stable;
            combined.declining += threadAgg.declining;
        }
    }

    // ZMIANA: Konwertuj ze skalowanego int na float (jak w GPU)
    if (simParams_.numProvinces > 0) {
        float scaledSum = static_cast<float>(combined.avgGrowth) / 100.0f;
        combined.avgGrowth = scaledSum / simParams_.numProvinces;
    }
    else {
        combined.avgGrowth = 0.0f;
    }

    // Store aggregates
    {
        std::lock_guard<std::mutex> lock(aggregatesMutex_);
        lastAggregates_ = combined;
    }

    // DODANE: Aktualizuj initialStats_ dla następnego ticku
    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        initialStats_[i] = sharedBuffer_->provinces[i];
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
    timings.readbackMs = 0.0;
    timings.totalMs = elapsedMs;

    {
        std::lock_guard<std::mutex> lock(timingsMutex_);
        lastStepTimings_ = timings;
    }

    tickCounter_.fetch_add(1, std::memory_order_relaxed);

    SPDLOG_TRACE("CPU: Step {} - Compute: {:.4f}ms, Pop: {}, Wealth: {}, AvgGrowth: {:.2f}%",
        tickCounter_.load(), elapsedMs, combined.totalPopulation,
        combined.totalWealth, combined.avgGrowth);
}

void CPUSimulationStrategy::setThreadCount(size_t threads) {
    threadCount_ = std::max(size_t(1), threads);
    SPDLOG_INFO("CPU: Thread count set to {}", threadCount_);
}

AggregateStatistics CPUSimulationStrategy::getAggregateStatistics() const {
    std::lock_guard<std::mutex> lock(aggregatesMutex_);
    return lastAggregates_;
}

// =============================================================================
// SIMULATION LOGIC WITH AGGREGATION
// =============================================================================

void CPUSimulationStrategy::simulateProvince(uint32_t idx, ThreadLocalAggregates& aggregates) {
    if (!sharedBuffer_ || idx >= simParams_.numProvinces) return;

    ProvinceData& province = sharedBuffer_->provinces[idx];
    const ProvinceData& previous = initialStats_[idx];  // Stan z poprzedniego ticku

    if (province.population < simParams_.minPopulation) {
        province.population = simParams_.minPopulation;
        province.foodStorage = 0;
        province.wealth = 0;

        // Update aggregates
        aggregates.totalPopulation += province.population;
        aggregates.totalWealth += province.wealth;
        aggregates.stable++;
        return;
    }

    // === FOOD PRODUCTION ===
    float foodProduced = province.foodProductionModifier;
    uint32_t foodToAdd = static_cast<uint32_t>(std::floor(foodProduced));
    province.foodStorage = std::min(province.foodStorage + foodToAdd, simParams_.maxFoodStorage);

    // === FOOD CONSUMPTION ===
    float foodNeeded = static_cast<float>(province.population) * simParams_.foodConsumptionPerPop;
    float foodConsumed = std::min(static_cast<float>(province.foodStorage), foodNeeded);
    province.foodStorage -= static_cast<uint32_t>(std::floor(foodConsumed));

    float foodRatio = foodConsumed / std::max(foodNeeded, 0.001f);

    // === POPULATION DYNAMICS ===
    float populationChangeFloat = 0.0f;

    if (foodRatio >= 1.0f) {
        float excessFood = foodConsumed - foodNeeded;
        float growthBonus = std::min(excessFood * 0.01f, 0.01f);
        populationChangeFloat = static_cast<float>(province.population) *
            (simParams_.basePopulationGrowth + growthBonus);
    }
    else if (foodRatio >= simParams_.starvationThreshold) {
        float partialGrowth = (foodRatio - simParams_.starvationThreshold) /
            (1.0f - simParams_.starvationThreshold);
        populationChangeFloat = static_cast<float>(province.population) *
            simParams_.basePopulationGrowth * partialGrowth;
    }
    else {
        float starvationSeverity = 1.0f - (foodRatio / simParams_.starvationThreshold);
        populationChangeFloat = -static_cast<float>(province.population) * 0.05f * starvationSeverity;
    }

    // Apply population change
    int32_t populationChange = static_cast<int32_t>(std::round(populationChangeFloat));
    int32_t newPopulation = static_cast<int32_t>(province.population) + populationChange;
    province.population = static_cast<uint32_t>(std::max(newPopulation,
        static_cast<int32_t>(simParams_.minPopulation)));

    // === WEALTH GENERATION ===
    if (foodRatio >= simParams_.starvationThreshold) {
        float wealthGenerated = static_cast<float>(province.population) *
            simParams_.wealthPerPop * foodRatio;
        province.wealth += static_cast<uint32_t>(std::floor(wealthGenerated));
    }

    // === UPDATE AGGREGATES ===
    aggregates.totalPopulation += province.population;
    aggregates.totalWealth += province.wealth;

    // Calculate growth percentage vs PREVIOUS tick
    if (previous.population > 0) {
        float growth = ((static_cast<float>(province.population) -
            static_cast<float>(previous.population)) /
            static_cast<float>(previous.population)) * 100.0f;

        // ZMIANA: Skaluj do int (jak w GPU)
        int32_t growthScaled = static_cast<int32_t>(std::round(growth * 100.0f));
        aggregates.sumGrowthScaled += growthScaled;

        if (growth > 0.1f) {
            aggregates.growing++;
        }
        else if (growth < -0.1f) {
            aggregates.declining++;
        }
        else {
            aggregates.stable++;
        }
    }
}

void CPUSimulationStrategy::computeAggregates() {
    AggregateStatistics aggregates{};
    int64_t sumGrowthScaled = 0;  // ZMIANA

    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        const auto& province = sharedBuffer_->provinces[i];
        const auto& initial = initialStats_[i];

        aggregates.totalPopulation += province.population;
        aggregates.totalWealth += province.wealth;

        if (initial.population > 0) {
            float growth = ((static_cast<float>(province.population) -
                static_cast<float>(initial.population)) /
                static_cast<float>(initial.population)) * 100.0f;

            // ZMIANA: Skaluj jak w GPU
            int32_t growthScaled = static_cast<int32_t>(std::round(growth * 100.0f));
            sumGrowthScaled += growthScaled;

            if (growth > 5.0f) aggregates.growing++;
            else if (growth < -5.0f) aggregates.declining++;
            else aggregates.stable++;
        }
    }

    // ZMIANA: Konwertuj ze skalowanego int
    if (simParams_.numProvinces > 0) {
        float scaledSum = static_cast<float>(sumGrowthScaled) / 100.0f;
        aggregates.avgGrowth = scaledSum / simParams_.numProvinces;
    }

    std::lock_guard<std::mutex> lock(aggregatesMutex_);
    lastAggregates_ = aggregates;
}
