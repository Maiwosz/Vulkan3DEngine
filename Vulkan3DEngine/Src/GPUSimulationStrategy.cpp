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

    // =========================================================================
    // START TIMING
    // =========================================================================
    auto startTime = std::chrono::high_resolution_clock::now();

    // =========================================================================
    // 1. DISPATCH COMPUTE WORK
    // =========================================================================
    ComputeTaskHandle task = dispatcher_->dispatchForDataSize(
        material_,
        simParams_.numProvinces,
        1, 1
    );

    if (!task) {
        SPDLOG_ERROR("GPU: Failed to dispatch compute");
        return;
    }

    // =========================================================================
    // 2. WAIT FOR COMPLETION (BLOCKING)
    // =========================================================================
    if (!dispatcher_->waitForTask(task)) {
        SPDLOG_ERROR("GPU: Failed to wait for task");
        return;
    }

    // =========================================================================
    // 3. READBACK TO CPU (if interval reached)
    // =========================================================================
    ticksSinceReadback_++;
    if (ticksSinceReadback_ >= readbackInterval_) {
        readbackFromGPU();
        ticksSinceReadback_ = 0;
    }

    // =========================================================================
    // STOP TIMING
    // =========================================================================
    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(
        endTime - startTime
    ).count();

    lastStepTimeMs_.store(elapsedMs, std::memory_order_relaxed);
    tickCounter_.fetch_add(1, std::memory_order_relaxed);

    SPDLOG_TRACE("GPU: Step {} completed in {:.4f}ms",
        tickCounter_.load(), elapsedMs);
}

// =============================================================================
// PRIVATE HELPERS
// =============================================================================

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
