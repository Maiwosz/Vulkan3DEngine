#include "ProvinceSimulationGPU.h"
#include "Engine.h"
#include <spdlog/spdlog.h>
#include <random>

ProvinceSimulationGPU::ProvinceSimulationGPU(ComputeDispatcher* dispatcher)
    : computeDispatcher_(dispatcher)
{
}

ProvinceSimulationGPU::~ProvinceSimulationGPU() {
    // Wait for any pending compute operations
    if (computeDispatcher_ && activeComputeTask_) {
        computeDispatcher_->waitForTask(*activeComputeTask_);
    }

    // Wait for any pending GPU reads
    if (material_ && activeGPUReadOp_) {
        auto inputOutputBuffer = material_->GetInputOutputBuffer();
        if (inputOutputBuffer) {
            inputOutputBuffer->WaitForSync(*activeGPUReadOp_);
        }
    }
}

bool ProvinceSimulationGPU::initialize(const SimulationParameters& simParams,
    const RandomizationParameters& randParams) {

    simParams_ = simParams;
    randParams_ = randParams;
    stepCounter_ = 0;
    pendingSteps_ = 0;
    state_ = SimulationState::Idle;

    if (!material_) {
        SPDLOG_ERROR("GPU: Material not initialized");
        return false;
    }

    // Allocate display cache (CPU-side only, for UI queries)
    if (!displayCache_) {
        displayCache_ = std::make_unique<ProvinceDataBuffer>();
    }

    // ====================================================================
    // STEP 1: Sync parameters to GPU (uniform buffer)
    // ====================================================================
    syncParametersToGPU();

    // ====================================================================
    // STEP 2: Initialize province data DIRECTLY on GPU
    // ====================================================================
    initializeGPUData();

    // ====================================================================
    // STEP 3: Read initial state for display cache
    // ====================================================================
    auto inputOutputBuffer = material_->GetInputOutputBuffer();
    if (!inputOutputBuffer) {
        SPDLOG_ERROR("GPU: InputOutput buffer not available");
        return false;
    }

    // Direct GPU -> CPU copy for initial stats
    inputOutputBuffer->CopyFromGPUDirect(
        displayCache_.get(),
        0,
        sizeof(ProvinceDataBuffer)
    );

    // Cache initial stats for UI
    initialStats_.resize(simParams_.numProvinces);
    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        initialStats_[i] = displayCache_->provinces[i];
    }

    SPDLOG_INFO("GPU Simulation initialized: {} provinces (direct GPU operations)",
        simParams_.numProvinces);

    return true;
}

void ProvinceSimulationGPU::runSingleStep() {
    if (state_ != SimulationState::Idle && state_ != SimulationState::ReadyForNextStep) {
        SPDLOG_WARN("GPU: Cannot run step, operation in progress");
        return;
    }

    pendingSteps_ = 0;
    dispatchNextStep();
}

void ProvinceSimulationGPU::runMultipleSteps(uint32_t numSteps) {
    if (state_ != SimulationState::Idle && state_ != SimulationState::ReadyForNextStep) {
        SPDLOG_WARN("GPU: Cannot run steps, operation in progress");
        return;
    }

    if (numSteps == 0) return;

    pendingSteps_ = numSteps - 1;
    dispatchNextStep();
}

void ProvinceSimulationGPU::reset() {
    // Wait for pending operations
    if (computeDispatcher_ && activeComputeTask_) {
        computeDispatcher_->waitForTask(*activeComputeTask_);
        activeComputeTask_.reset();
    }

    if (material_ && activeGPUReadOp_) {
        auto inputOutputBuffer = material_->GetInputOutputBuffer();
        if (inputOutputBuffer) {
            inputOutputBuffer->WaitForSync(*activeGPUReadOp_);
        }
        activeGPUReadOp_.reset();
    }

    // Reinitialize
    initialize(simParams_, randParams_);
}

bool ProvinceSimulationGPU::isComputeInProgress() const {
    if (!computeDispatcher_ || !activeComputeTask_) return false;
    return !computeDispatcher_->isTaskComplete(*activeComputeTask_);
}

ProvinceData ProvinceSimulationGPU::getProvinceData(uint32_t index) const {
    if (!displayCache_ || index >= simParams_.numProvinces) {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    return displayCache_->provinces[index];
}

ProvinceData ProvinceSimulationGPU::getInitialStats(uint32_t index) const {
    if (index >= initialStats_.size()) {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    return initialStats_[index];
}

void ProvinceSimulationGPU::setSimulationParameters(const SimulationParameters& params) {
    if (state_ != SimulationState::Idle) {
        SPDLOG_WARN("GPU: Cannot change parameters during operation");
        return;
    }
    simParams_ = params;
    syncParametersToGPU();
}

void ProvinceSimulationGPU::setRandomizationParameters(const RandomizationParameters& params) {
    if (state_ != SimulationState::Idle) {
        SPDLOG_WARN("GPU: Cannot change parameters during operation");
        return;
    }
    randParams_ = params;
}

void ProvinceSimulationGPU::requestDataRefresh() {
    if (state_ != SimulationState::ReadyForNextStep && state_ != SimulationState::Idle) {
        SPDLOG_WARN("GPU: Cannot refresh data, operation in progress");
        return;
    }
    startGPURead();
}

bool ProvinceSimulationGPU::isDataRefreshComplete() const {
    if (!activeGPUReadOp_) return true;
    if (!material_) return false;

    auto inputOutputBuffer = material_->GetInputOutputBuffer();
    if (!inputOutputBuffer) return false;

    return inputOutputBuffer->IsSyncComplete(*activeGPUReadOp_);
}

void ProvinceSimulationGPU::update() {
    updateStateMachine();
}

// =============================================================================
// STATE MACHINE
// =============================================================================

void ProvinceSimulationGPU::updateStateMachine() {
    switch (state_) {
    case SimulationState::Computing:
        handleComputingState();
        break;
    case SimulationState::ReadingFromGPU:
        handleReadingState();
        break;
    case SimulationState::ReadyForNextStep:
        handleReadyState();
        break;
    case SimulationState::Idle:
        break;
    }
}

void ProvinceSimulationGPU::handleComputingState() {
    if (!activeComputeTask_ || !computeDispatcher_) {
        state_ = SimulationState::Idle;
        return;
    }

    if (computeDispatcher_->isTaskComplete(*activeComputeTask_)) {
        activeComputeTask_.reset();
        stepCounter_++;

        // Start async GPU read for display cache
        startGPURead();
    }
}

void ProvinceSimulationGPU::handleReadingState() {
    if (!material_ || !activeGPUReadOp_) {
        state_ = SimulationState::Idle;
        return;
    }

    auto inputOutputBuffer = material_->GetInputOutputBuffer();
    if (!inputOutputBuffer) {
        state_ = SimulationState::Idle;
        return;
    }

    // Check if GPU read completed
    if (inputOutputBuffer->IsSyncComplete(*activeGPUReadOp_)) {
        // GPU read finished, no need to update cache (already done by async op)
        activeGPUReadOp_.reset();
        state_ = SimulationState::ReadyForNextStep;
    }
}

void ProvinceSimulationGPU::handleReadyState() {
    if (pendingSteps_ > 0) {
        pendingSteps_--;
        dispatchNextStep();
    }
    else {
        state_ = SimulationState::Idle;
    }
}

// =============================================================================
// OPERATIONS
// =============================================================================

bool ProvinceSimulationGPU::dispatchNextStep() {
    if (!computeDispatcher_ || !material_) return false;

    ComputeTaskHandle task = computeDispatcher_->dispatchForDataSize(
        material_,
        simParams_.numProvinces,
        1,
        1
    );

    if (!task) {
        SPDLOG_ERROR("GPU: Failed to dispatch compute");
        state_ = SimulationState::Idle;
        return false;
    }

    activeComputeTask_ = task;
    state_ = SimulationState::Computing;
    return true;
}

void ProvinceSimulationGPU::startGPURead() {
    if (!material_) return;

    auto inputOutputBuffer = material_->GetInputOutputBuffer();
    if (!inputOutputBuffer) {
        SPDLOG_ERROR("GPU: InputOutput buffer not available");
        return;
    }

    // Start async GPU -> CPU copy directly to display cache
    // This bypasses the BufferObjectInstance's internal CPU buffer entirely
    activeGPUReadOp_ = inputOutputBuffer->CopyFromGPUDirectAsync(
        displayCache_.get(),
        0,
        sizeof(ProvinceDataBuffer)
    );

    if (!activeGPUReadOp_ || !activeGPUReadOp_->isValid()) {
        SPDLOG_ERROR("GPU: Failed to start async GPU read");
        state_ = SimulationState::Idle;
        return;
    }

    state_ = SimulationState::ReadingFromGPU;

    SPDLOG_TRACE("GPU: Started async read ({} bytes)", sizeof(ProvinceDataBuffer));
}

// =============================================================================
// INITIALIZATION HELPERS
// =============================================================================

void ProvinceSimulationGPU::initializeGPUData() {
    if (!material_) return;

    auto inputOutputBuffer = material_->GetInputOutputBuffer();
    if (!inputOutputBuffer) {
        SPDLOG_ERROR("GPU: InputOutput buffer not available");
        return;
    }

    // Generate random data on CPU
    std::mt19937 gen;
    if (randParams_.randomSeed == 0) {
        std::random_device rd;
        gen.seed(rd());
    }
    else {
        gen.seed(randParams_.randomSeed);
    }

    std::uniform_real_distribution<float> popDist(
        randParams_.minPopulation,
        randParams_.maxPopulation
    );
    std::uniform_real_distribution<float> foodProdDist(
        randParams_.minFoodProduction,
        randParams_.maxFoodProduction
    );

    // Prepare temporary buffer
    auto tempBuffer = std::make_unique<ProvinceDataBuffer>();

    // Initialize active provinces
    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        tempBuffer->provinces[i] = {
            popDist(gen),
            foodProdDist(gen),
            0.0f,
            randParams_.initialFoodStorage
        };
    }

    // Zero unused provinces
    for (uint32_t i = simParams_.numProvinces; i < MAX_PROVINCES; ++i) {
        tempBuffer->provinces[i] = { 0.0f, 0.0f, 0.0f, 0.0f };
    }

    // Direct CPU -> GPU copy (blocking, but only happens during initialization)
    inputOutputBuffer->CopyToGPUDirect(
        tempBuffer.get(),
        0,
        sizeof(ProvinceDataBuffer)
    );

    SPDLOG_DEBUG("Initialized {} provinces with direct GPU write ({} bytes)",
        simParams_.numProvinces, sizeof(ProvinceDataBuffer));
}

void ProvinceSimulationGPU::syncParametersToGPU() {
    if (!material_) return;

    auto inputBuffer = material_->GetInputBuffer();
    if (!inputBuffer) {
        SPDLOG_ERROR("GPU: Input buffer not available");
        return;
    }

    // Direct CPU -> GPU write for parameters (small, uniform buffer)
    inputBuffer->CopyToGPUDirect(
        &simParams_,
        0,
        sizeof(SimulationParameters)
    );

    SPDLOG_DEBUG("Synced simulation parameters to GPU ({} bytes)",
        sizeof(SimulationParameters));
}
