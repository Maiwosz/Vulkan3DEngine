#include "GPUSimulationStrategy.h"
#include <spdlog/spdlog.h>
#include <random>
#include <chrono>

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

    // Initial readback to shared buffer
    readbackFromGPU();

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
    // 1. COMPUTE PHASE
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
    // 2. READBACK PHASE (if auto-readback enabled)
    // =========================================================================
    timings.readbackMs = 0.0;

    if (autoReadback_) {
        ticksSinceReadback_++;
        if (ticksSinceReadback_ >= readbackInterval_) {
            auto readbackStart = std::chrono::high_resolution_clock::now();

            readbackFromGPU();

            auto readbackEnd = std::chrono::high_resolution_clock::now();
            timings.readbackMs = std::chrono::duration<double, std::milli>(
                readbackEnd - readbackStart
            ).count();

            ticksSinceReadback_ = 0;
        }
    }

    // =========================================================================
    // 3. TOTAL TIME
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
// PRIVATE HELPERS
// =============================================================================

void GPUSimulationStrategy::manualReadback() {
    auto readbackStart = std::chrono::high_resolution_clock::now();

    readbackFromGPU();

    auto readbackEnd = std::chrono::high_resolution_clock::now();
    double readbackMs = std::chrono::duration<double, std::milli>(
        readbackEnd - readbackStart
    ).count();

    SPDLOG_DEBUG("GPU: Manual readback completed in {:.4f}ms", readbackMs);

    ticksSinceReadback_ = 0;
}

void GPUSimulationStrategy::initializeGPUData(
    const RandomizationParameters& randParams
) {
    auto inputOutputBuffer = material_->GetInputOutputBuffer();
    if (!inputOutputBuffer) {
        SPDLOG_ERROR("GPU: InputOutput buffer not available");
        return;
    }

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

    auto tempBuffer = std::make_unique<ProvinceDataBuffer>();

    // Initialize active provinces
    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        tempBuffer->provinces[i] = {
            popDist(gen),
            foodProdDist(gen),
            0.0f,  // wealth starts at 0
            randParams.initialFoodStorage
        };
    }

    // Zero unused provinces (optional, for safety)
    for (uint32_t i = simParams_.numProvinces; i < MAX_PROVINCES; ++i) {
        tempBuffer->provinces[i] = { 0.0f, 0.0f, 0.0f, 0.0f };
    }

    // Upload only active data to GPU (BLOCKING)
    const uint32_t uploadSize = simParams_.numProvinces * sizeof(ProvinceData);
    inputOutputBuffer->CopyToGPUDirect(
        tempBuffer.get(),
        0,
        uploadSize
    );

    SPDLOG_DEBUG("GPU: Initialized {} provinces on GPU ({} bytes)",
        simParams_.numProvinces, uploadSize);
}

void GPUSimulationStrategy::syncParametersToGPU() {
    auto inputBuffer = material_->GetInputBuffer();
    if (!inputBuffer) {
        SPDLOG_ERROR("GPU: Input buffer not available");
        return;
    }

    // Upload parameters (BLOCKING)
    inputBuffer->CopyToGPUDirect(
        &simParams_,
        0,
        sizeof(SimulationParameters)
    );

    SPDLOG_DEBUG("GPU: Synced parameters to GPU");
}

void GPUSimulationStrategy::readbackFromGPU() {
    auto inputOutputBuffer = material_->GetInputOutputBuffer();
    if (!inputOutputBuffer || !sharedBuffer_) {
        SPDLOG_ERROR("GPU: Cannot perform readback");
        return;
    }

    // Calculate actual data size (only active provinces)
    const uint32_t activeDataSize = simParams_.numProvinces * sizeof(ProvinceData);

    // Readback only active data from GPU (BLOCKING)
    inputOutputBuffer->CopyFromGPUDirect(
        sharedBuffer_,
        0,
        activeDataSize
    );

    SPDLOG_TRACE("GPU: Readback completed ({} provinces, {} bytes)",
        simParams_.numProvinces, activeDataSize);
}
