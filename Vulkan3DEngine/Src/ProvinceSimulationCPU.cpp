#include "ProvinceSimulationCPU.h"
#include <spdlog/spdlog.h>
#include <random>
#include <algorithm>
#include <cmath>

ProvinceSimulationCPU::ProvinceSimulationCPU(ThreadPool* threadPool, AsyncMemoryOps* asyncMemOps)
    : threadPool_(threadPool)
    , asyncMemOps_(asyncMemOps)
    , threadCount_(threadPool ? threadPool->getThreadCount() : 1)
{
}

ProvinceSimulationCPU::~ProvinceSimulationCPU() {
    waitForCompletion();
}

bool ProvinceSimulationCPU::initialize(const SimulationParameters& simParams,
    const RandomizationParameters& randParams) {
    waitForCompletion();

    simParams_ = simParams;
    randParams_ = randParams;
    stepCounter_ = 0;
    pendingSteps_ = 0;

    // Allocate working buffer
    if (!workingBuffer_) {
        workingBuffer_ = std::make_unique<ProvinceDataBuffer>();
    }

    // Initialize with random data
    std::mt19937 gen;
    if (randParams_.randomSeed == 0) {
        std::random_device rd;
        gen.seed(rd());
    }
    else {
        gen.seed(randParams_.randomSeed);
    }

    std::uniform_real_distribution<float> popDist(randParams_.minPopulation, randParams_.maxPopulation);
    std::uniform_real_distribution<float> foodProdDist(randParams_.minFoodProduction, randParams_.maxFoodProduction);

    // Initialize active provinces
    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        workingBuffer_->provinces[i] = {
            popDist(gen),
            foodProdDist(gen),
            0.0f,
            randParams_.initialFoodStorage
        };
    }

    // Zero unused provinces
    for (uint32_t i = simParams_.numProvinces; i < MAX_PROVINCES; ++i) {
        workingBuffer_->provinces[i] = { 0.0f, 0.0f, 0.0f, 0.0f };
    }

    // Cache initial stats
    initialStats_.resize(simParams_.numProvinces);
    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        initialStats_[i] = workingBuffer_->provinces[i];
    }

    SPDLOG_INFO("CPU Simulation initialized: {} provinces, {} threads",
        simParams_.numProvinces, threadCount_);

    return true;
}

void ProvinceSimulationCPU::runSingleStep() {
    if (computeInProgress_) {
        SPDLOG_WARN("CPU: Cannot run step, computation already in progress");
        return;
    }

    pendingSteps_ = 0;
    dispatchCompute();
}

void ProvinceSimulationCPU::runMultipleSteps(uint32_t numSteps) {
    if (computeInProgress_) {
        SPDLOG_WARN("CPU: Cannot run steps, computation already in progress");
        return;
    }

    if (numSteps == 0) return;

    pendingSteps_ = numSteps - 1;
    dispatchCompute();
}

void ProvinceSimulationCPU::reset() {
    waitForCompletion();
    initialize(simParams_, randParams_);
}

bool ProvinceSimulationCPU::isComputeInProgress() const {
    return computeInProgress_.load();
}

ProvinceData ProvinceSimulationCPU::getProvinceData(uint32_t index) const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    if (index >= simParams_.numProvinces || !workingBuffer_) {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    return workingBuffer_->provinces[index];
}

ProvinceData ProvinceSimulationCPU::getInitialStats(uint32_t index) const {
    if (index >= initialStats_.size()) {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    return initialStats_[index];
}

void ProvinceSimulationCPU::setSimulationParameters(const SimulationParameters& params) {
    if (computeInProgress_) {
        SPDLOG_WARN("CPU: Cannot change parameters during computation");
        return;
    }
    simParams_ = params;
}

void ProvinceSimulationCPU::setRandomizationParameters(const RandomizationParameters& params) {
    if (computeInProgress_) {
        SPDLOG_WARN("CPU: Cannot change parameters during computation");
        return;
    }
    randParams_ = params;
}

void ProvinceSimulationCPU::setThreadCount(size_t threads) {
    threadCount_ = std::max(size_t(1), threads);
}

void ProvinceSimulationCPU::dispatchCompute() {
    if (!threadPool_ || !workingBuffer_) {
        SPDLOG_ERROR("CPU: ThreadPool or buffer not available");
        return;
    }

    computeInProgress_ = true;
    activeOperationHandle_ = ShaderLib::AsyncOperationHandle();

    // Split work across threads using AsyncMemoryOps::ExecuteBatch
    if (asyncMemOps_) {
        // Use AsyncMemoryOps for optimal work distribution
        activeOperationHandle_ = asyncMemOps_->ExecuteBatch(
            simParams_.numProvinces,
            [this](size_t provinceIdx) {
                simulateSingleProvince(static_cast<uint32_t>(provinceIdx));
            }
        );
    }
    else {
        // Fallback: manual distribution using ThreadPool directly
        activeFutures_.clear();
        uint32_t provincesPerThread = (simParams_.numProvinces + threadCount_ - 1) / threadCount_;

        for (size_t i = 0; i < threadCount_; ++i) {
            uint32_t startIdx = i * provincesPerThread;
            uint32_t endIdx = std::min(startIdx + provincesPerThread, simParams_.numProvinces);

            if (startIdx >= endIdx) break;

            auto future = threadPool_->enqueue([this, startIdx, endIdx]() {
                simulateProvinceRange(startIdx, endIdx);
                });

            activeFutures_.push_back(std::move(future));
        }
    }

    // Launch async completion handler
    threadPool_->enqueue([this]() {
        // Wait for all worker threads
        if (asyncMemOps_ && activeOperationHandle_.isValid()) {
            asyncMemOps_->WaitForOperation(activeOperationHandle_);
            activeOperationHandle_ = ShaderLib::AsyncOperationHandle();
        }
        else {
            // Wait for manual futures
            for (auto& future : activeFutures_) {
                if (future.valid()) {
                    try {
                        future.get();
                    }
                    catch (const std::exception& e) {
                        SPDLOG_ERROR("CPU Simulation: Exception in worker thread: {}", e.what());
                    }
                }
            }
            activeFutures_.clear();
        }

        stepCounter_++;

        // Check if more steps pending
        uint32_t remaining = pendingSteps_.load();
        if (remaining > 0) {
            pendingSteps_--;
            computeInProgress_ = false;
            dispatchCompute();
        }
        else {
            computeInProgress_ = false;
        }
        });
}

void ProvinceSimulationCPU::waitForCompletion() {
    while (computeInProgress_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void ProvinceSimulationCPU::simulateProvinceRange(uint32_t startIdx, uint32_t endIdx) {
    for (uint32_t i = startIdx; i < endIdx; ++i) {
        simulateSingleProvince(i);
    }
}

void ProvinceSimulationCPU::simulateSingleProvince(uint32_t idx) {
    std::lock_guard<std::mutex> lock(dataMutex_);

    if (!workingBuffer_ || idx >= simParams_.numProvinces) return;

    ProvinceData& province = workingBuffer_->provinces[idx];

    // Skip if population negligible
    if (province.population < simParams_.minPopulation) {
        province.population = simParams_.minPopulation;
        province.foodStorage = 0.0f;
        province.wealth = 0.0f;
        return;
    }

    // === FOOD PRODUCTION ===
    float foodProduced = province.foodProductionModifier;
    province.foodStorage += foodProduced;
    province.foodStorage = std::min(province.foodStorage, simParams_.maxFoodStorage);

    // === FOOD CONSUMPTION ===
    float foodNeeded = province.population * simParams_.foodConsumptionPerPop;
    float foodConsumed = std::min(province.foodStorage, foodNeeded);
    province.foodStorage -= foodConsumed;

    float foodRatio = foodConsumed / std::max(foodNeeded, 0.001f);

    // === POPULATION DYNAMICS ===
    float populationChange = 0.0f;

    if (foodRatio >= 1.0f) {
        // Well-fed: population grows
        float excessFood = foodConsumed - foodNeeded;
        float growthBonus = std::min(excessFood * 0.01f, 0.01f);
        populationChange = province.population * (simParams_.basePopulationGrowth + growthBonus);
    }
    else if (foodRatio >= simParams_.starvationThreshold) {
        // Moderate food: slow growth or stability
        float partialGrowth = (foodRatio - simParams_.starvationThreshold) /
            (1.0f - simParams_.starvationThreshold);
        populationChange = province.population * simParams_.basePopulationGrowth * partialGrowth;
    }
    else {
        // Starvation: population declines
        float starvationSeverity = 1.0f - (foodRatio / simParams_.starvationThreshold);
        populationChange = -province.population * 0.05f * starvationSeverity;
    }

    province.population += populationChange;
    province.population = std::max(province.population, simParams_.minPopulation);

    // === WEALTH GENERATION ===
    if (foodRatio >= simParams_.starvationThreshold) {
        float wealthGenerated = province.population * simParams_.wealthPerPop * foodRatio;
        province.wealth += wealthGenerated;
    }
}
