#include "GPUSimulationStrategy.h"
#include <spdlog/spdlog.h>
#include <random>
#include <chrono>
#include <algorithm>

GPUSimulationStrategy::GPUSimulationStrategy(
    ComputeDispatcher* dispatcher,
    MaterialManager* materialMgr
)
    : dispatcher_(dispatcher)
    , materialManager_(materialMgr)
{
}

GPUSimulationStrategy::~GPUSimulationStrategy() {
    shutdown();
}

bool GPUSimulationStrategy::initialize(
    const SimulationParameters& simParams,
    const RandomizationParameters& randParams,
    ProvinceDataBuffer* sharedBuffer
) {
    if (!dispatcher_ || !materialManager_ || !sharedBuffer) {
        SPDLOG_ERROR("GPU: Invalid initialization parameters");
        return false;
    }

    simParams_ = simParams;
    sharedBuffer_ = sharedBuffer;
    tickCounter_ = 0;
    ticksSinceReadback_ = 0;

    // Create material
    material_ = materialManager_->createComputeMaterial("ProvinceSimulation");
    if (!material_) {
        SPDLOG_ERROR("GPU: Failed to create compute material");
        return false;
    }

    // Initialize GPU data
    initializeGPUData(randParams);
    syncParametersToGPU();

    // Store initial stats for growth calculation
    initialStats_.resize(simParams_.numProvinces);
    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        initialStats_[i] = sharedBuffer_->provinces[i];
    }

    // Initial readback
    readbackFromGPU();
    readbackGPUAggregates();

    SPDLOG_INFO("GPU strategy initialized: {} provinces", simParams_.numProvinces);
    return true;
}

void GPUSimulationStrategy::shutdown() {
    material_.reset();
}

void GPUSimulationStrategy::executeSingleStep() {
    if (!material_ || !dispatcher_) {
        SPDLOG_ERROR("GPU: Strategy not initialized");
        return;
    }

    StepTimings timings;

    // =========================================================================
    // 1. CLEAR OUTPUT BUFFER (GPU Aggregates)
    // =========================================================================
    auto outputBuffer = material_->GetBuffer("OutputData");
    outputBuffer->Zero();
    outputBuffer->SyncToBuffer();

    // =========================================================================
    // 2. COMPUTE PHASE
    // =========================================================================
    auto computeStart = std::chrono::high_resolution_clock::now();

    ComputeTaskHandle task = dispatcher_->dispatchForDataSize(
        material_,
        simParams_.numProvinces,
        1, 1
    );

    if (!task) {
        SPDLOG_ERROR("GPU: Failed to dispatch compute");
        return;
    }

    if (!dispatcher_->waitForTask(task)) {
        SPDLOG_ERROR("GPU: Failed to wait for task");
        return;
    }

    auto computeEnd = std::chrono::high_resolution_clock::now();
    timings.computeMs = std::chrono::duration<double, std::milli>(
        computeEnd - computeStart
    ).count();

    // =========================================================================
    // 3. READBACK PHASE
    // =========================================================================
    timings.readbackMs = 0.0;

    if (autoReadback_) {
        ticksSinceReadback_++;
        if (ticksSinceReadback_ >= readbackInterval_) {
            auto readbackStart = std::chrono::high_resolution_clock::now();

            if (readbackFullData_) {
                // Mode 1: Full readback + CPU aggregation
                readbackFromGPU();

                // DODANE: Aktualizuj initialStats_ przed agregacją
                for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
                    initialStats_[i] = sharedBuffer_->provinces[i];
                }

                computeCPUAggregates();
            }
            else {
                // Mode 2: GPU aggregates only (fast)
                readbackGPUAggregates();
            }

            auto readbackEnd = std::chrono::high_resolution_clock::now();
            timings.readbackMs = std::chrono::duration<double, std::milli>(
                readbackEnd - readbackStart
            ).count();

            ticksSinceReadback_ = 0;
        }
    }

    // =========================================================================
    // 4. TOTAL TIME
    // =========================================================================
    timings.totalMs = timings.computeMs + timings.readbackMs;

    {
        std::lock_guard<std::mutex> lock(timingsMutex_);
        lastStepTimings_ = timings;
    }

    tickCounter_.fetch_add(1, std::memory_order_relaxed);

    SPDLOG_TRACE("GPU: Step {} - Compute: {:.4f}ms, Readback: {:.4f}ms, Total: {:.4f}ms",
        tickCounter_.load(), timings.computeMs, timings.readbackMs, timings.totalMs);
}

// =============================================================================
// AGGREGATE STATISTICS
// =============================================================================

AggregateStatistics GPUSimulationStrategy::getAggregateStatistics() const {
    std::lock_guard<std::mutex> lock(aggregatesMutex_);
    return lastAggregates_;
}

void GPUSimulationStrategy::setReadbackFullData(bool enabled) {
    readbackFullData_ = enabled;

    // Zaktualizuj parametry symulacji
    simParams_.enableGPUAggregation = enabled ? 0 : 1;
    syncParametersToGPU();
}

void GPUSimulationStrategy::readbackGPUAggregates() {
    auto outputBuffer = material_->GetBuffer("OutputData");
    if (!outputBuffer) {
        SPDLOG_ERROR("GPU: Cannot readback aggregates");
        return;
    }

    GPUAggregateData gpuAggregates;
    outputBuffer->CopyFromGPUDirect(
        &gpuAggregates,
        0,
        sizeof(GPUAggregateData)
    );

    AggregateStatistics aggregates;
    aggregates.totalPopulation = gpuAggregates.totalPopulation;
    aggregates.totalWealth = gpuAggregates.totalWealth;
    aggregates.growing = gpuAggregates.growing;
    aggregates.stable = gpuAggregates.stable;
    aggregates.declining = gpuAggregates.declining;

    // Konwersja ze skalowanego int z powrotem na float
    if (simParams_.numProvinces > 0) {
        float scaledSum = static_cast<float>(gpuAggregates.avgGrowthScaled) / 100.0f;
        aggregates.avgGrowth = scaledSum / simParams_.numProvinces;
    }
    else {
        aggregates.avgGrowth = 0.0f;
    }

    std::lock_guard<std::mutex> lock(aggregatesMutex_);
    lastAggregates_ = aggregates;

    SPDLOG_TRACE("GPU: GPU Aggregates - Pop: {}, Wealth: {}, AvgGrowth: {:.2f}%, Growing: {}, Stable: {}, Declining: {}",
        aggregates.totalPopulation,
        aggregates.totalWealth,
        aggregates.avgGrowth,
        aggregates.growing,
        aggregates.stable,
        aggregates.declining);
}

void GPUSimulationStrategy::computeCPUAggregates() {
    if (!sharedBuffer_) {
        SPDLOG_ERROR("GPU: Cannot compute CPU aggregates - no shared buffer");
        return;
    }

    AggregateStatistics aggregates{};
    int64_t sumGrowthScaled = 0;  // ZMIANA: int64_t zamiast sumowania do float

    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        const auto& province = sharedBuffer_->provinces[i];
        const auto& previous = initialStats_[i];

        aggregates.totalPopulation += province.population;
        aggregates.totalWealth += province.wealth;

        if (previous.population > 0) {
            float growth = ((static_cast<float>(province.population) -
                static_cast<float>(previous.population)) /
                static_cast<float>(previous.population)) * 100.0f;

            // ZMIANA: Skaluj do int (jak w GPU shader i CPU strategy)
            int32_t growthScaled = static_cast<int32_t>(std::round(growth * 100.0f));
            sumGrowthScaled += growthScaled;

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

    // ZMIANA: Konwertuj ze skalowanego int na float (jak w GPU)
    if (simParams_.numProvinces > 0) {
        float scaledSum = static_cast<float>(sumGrowthScaled) / 100.0f;
        aggregates.avgGrowth = scaledSum / simParams_.numProvinces;
    }
    else {
        aggregates.avgGrowth = 0.0f;
    }

    std::lock_guard<std::mutex> lock(aggregatesMutex_);
    lastAggregates_ = aggregates;

    SPDLOG_TRACE("GPU: CPU Aggregates - Pop: {}, Wealth: {}, AvgGrowth: {:.2f}%, Growing: {}, Stable: {}, Declining: {}",
        aggregates.totalPopulation,
        aggregates.totalWealth,
        aggregates.avgGrowth,
        aggregates.growing,
        aggregates.stable,
        aggregates.declining);
}

// =============================================================================
// PROVINCE DATA ACCESS
// =============================================================================

ProvinceData GPUSimulationStrategy::getProvinceData(uint32_t index) {
    auto inputOutputBuffer = material_->GetBuffer("InputOutputData");
    if (!inputOutputBuffer || index >= simParams_.numProvinces) {
        SPDLOG_ERROR("GPU: Cannot read province data");
        return { 0, 0.0f, 0, 0 };
    }

    ProvinceData data;
    const uint32_t offset = index * sizeof(ProvinceData);
    inputOutputBuffer->CopyFromGPUDirect(
        &data,
        offset,
        sizeof(ProvinceData)
    );

    return data;
}

// =============================================================================
// PRIVATE HELPERS
// =============================================================================

void GPUSimulationStrategy::manualReadback() {
    auto readbackStart = std::chrono::high_resolution_clock::now();

    if (readbackFullData_) {
        readbackFromGPU();
        computeCPUAggregates();
    }
    else {
        readbackGPUAggregates();
    }

    auto readbackEnd = std::chrono::high_resolution_clock::now();
    double readbackMs = std::chrono::duration<double, std::milli>(
        readbackEnd - readbackStart
    ).count();

    SPDLOG_DEBUG("GPU: Manual readback completed in {:.4f}ms", readbackMs);

    ticksSinceReadback_ = 0;
}

void GPUSimulationStrategy::uploadSingleProvince(uint32_t index, const ProvinceData& data) {
    auto inputOutputBuffer = material_->GetBuffer("InputOutputData");
    if (!inputOutputBuffer || index >= simParams_.numProvinces) {
        SPDLOG_ERROR("GPU: Cannot upload province data");
        return;
    }

    const uint32_t offset = index * sizeof(ProvinceData);
    inputOutputBuffer->CopyToGPUDirect(
        &data,
        offset,
        sizeof(ProvinceData)
    );

    SPDLOG_DEBUG("GPU: Uploaded data for province {}", index);
}

void GPUSimulationStrategy::initializeGPUData(
    const RandomizationParameters& randParams
) {
    auto inputOutputBuffer = material_->GetBuffer("InputOutputData");
    if (!inputOutputBuffer) {
        SPDLOG_ERROR("GPU: InputOutput buffer not available");
        return;
    }

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

    auto tempBuffer = std::make_unique<ProvinceDataBuffer>();

    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        tempBuffer->provinces[i] = {
            popDist(gen),
            foodProdDist(gen),
            0,
            randParams.initialFoodStorage
        };
    }

    for (uint32_t i = simParams_.numProvinces; i < MAX_PROVINCES; ++i) {
        tempBuffer->provinces[i] = { 0, 0.0f, 0, 0 };
    }

    const uint32_t uploadSize = simParams_.numProvinces * sizeof(ProvinceData);
    inputOutputBuffer->CopyToGPUDirect(
        tempBuffer.get(),
        0,
        uploadSize
    );

    // Also copy to shared buffer for initial stats
    std::memcpy(sharedBuffer_, tempBuffer.get(), uploadSize);

    SPDLOG_DEBUG("GPU: Initialized {} provinces on GPU ({} bytes)",
        simParams_.numProvinces, uploadSize);
}

void GPUSimulationStrategy::syncParametersToGPU() {
    auto inputBuffer = material_->GetBuffer("InputData");
    if (!inputBuffer) {
        SPDLOG_ERROR("GPU: Input buffer not available");
        return;
    }

    inputBuffer->CopyToGPUDirect(
        &simParams_,
        0,
        sizeof(SimulationParameters)
    );

    SPDLOG_DEBUG("GPU: Synced parameters to GPU");
}

void GPUSimulationStrategy::readbackFromGPU() {
    auto inputOutputBuffer = material_->GetBuffer("InputOutputData");
    if (!inputOutputBuffer || !sharedBuffer_) {
        SPDLOG_ERROR("GPU: Cannot perform readback");
        return;
    }

    const uint32_t activeDataSize = simParams_.numProvinces * sizeof(ProvinceData);
    inputOutputBuffer->CopyFromGPUDirect(
        sharedBuffer_,
        0,
        activeDataSize
    );

    SPDLOG_TRACE("GPU: Full readback completed ({} provinces, {} bytes)",
        simParams_.numProvinces, activeDataSize);
}
