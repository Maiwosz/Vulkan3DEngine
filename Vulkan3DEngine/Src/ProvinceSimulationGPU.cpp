#include "ProvinceSimulationGPU.h"
#include "Engine.h"
#include <spdlog/spdlog.h>
#include <random>

ProvinceSimulationGPU::ProvinceSimulationGPU(ComputeDispatcher* dispatcher, AsyncMemoryOps* asyncMemOps)
    : computeDispatcher_(dispatcher)
    , asyncMemOps_(asyncMemOps)
{
}

ProvinceSimulationGPU::~ProvinceSimulationGPU() {
    if (computeDispatcher_ && activeComputeTask_) {
        computeDispatcher_->waitForTask(*activeComputeTask_);
    }
    if (material_ && !activeSyncTasks_.empty()) {
        material_->WaitForTasks(activeSyncTasks_);
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

    // Allocate working buffer
    if (!workingBuffer_) {
        workingBuffer_ = std::make_unique<ProvinceDataBuffer>();
    }

    // ====================================================================
    // STEP 1: Prepare input parameters (uniform buffer)
    // ====================================================================
    syncParametersToMaterial();

    // ====================================================================
    // STEP 2: Initialize province data with random values
    // ====================================================================
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

    // ====================================================================
    // STEP 3: Copy to GPU buffer using AsyncMemoryOps
    // ====================================================================
    auto inputOutputBuffer = material_->GetInputOutputBuffer();
    if (!inputOutputBuffer) {
        SPDLOG_ERROR("GPU: InputOutput buffer not available");
        return false;
    }

    uint8_t* gpuBuffer = inputOutputBuffer->GetRawBuffer();
    const size_t bufferSize = sizeof(ProvinceDataBuffer);

    if (asyncMemOps_) {
        // Asynchronous, multi-threaded copy
        auto copyFutures = asyncMemOps_->Memcpy(
            gpuBuffer,
            workingBuffer_.get(),
            bufferSize
        );

        SPDLOG_DEBUG("Initialized {} provinces using async memcpy ({} bytes, {} chunks)",
            simParams_.numProvinces, bufferSize, copyFutures.size());

        // Wait for copy completion
        AsyncMemoryOps::WaitForAll(copyFutures);
    }
    else {
        // Fallback: synchronous copy
        std::memcpy(gpuBuffer, workingBuffer_.get(), bufferSize);

        SPDLOG_DEBUG("Initialized {} provinces using sync memcpy ({} bytes)",
            simParams_.numProvinces, bufferSize);
    }

    // Sync to GPU
    material_->SyncToGPU();

    // ====================================================================
    // STEP 4: Cache initial stats for UI display
    // ====================================================================
    initialStats_.resize(simParams_.numProvinces);
    cachedStats_.resize(simParams_.numProvinces);

    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        initialStats_[i] = workingBuffer_->provinces[i];
        cachedStats_[i] = initialStats_[i];
    }

    SPDLOG_INFO("GPU Simulation initialized: {} provinces", simParams_.numProvinces);

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
    if (computeDispatcher_ && activeComputeTask_) {
        computeDispatcher_->waitForTask(*activeComputeTask_);
        activeComputeTask_.reset();
    }

    if (material_ && !activeSyncTasks_.empty()) {
        material_->WaitForTasks(activeSyncTasks_);
        activeSyncTasks_.clear();
    }

    initialize(simParams_, randParams_);
}

bool ProvinceSimulationGPU::isComputeInProgress() const {
    if (!computeDispatcher_ || !activeComputeTask_) return false;
    return !computeDispatcher_->isTaskComplete(*activeComputeTask_);
}

ProvinceData ProvinceSimulationGPU::getProvinceData(uint32_t index) const {
    if (index >= cachedStats_.size()) {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    return cachedStats_[index];
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
    syncParametersToMaterial();
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
    syncDataFromGPU();
}

bool ProvinceSimulationGPU::isDataRefreshComplete() const {
    if (activeSyncTasks_.empty()) return true;
    if (!material_) return false;
    return material_->AreTasksComplete(activeSyncTasks_);
}

void ProvinceSimulationGPU::update() {
    updateStateMachine();
}

void ProvinceSimulationGPU::updateStateMachine() {
    switch (state_) {
    case SimulationState::Computing:
        handleComputingState();
        break;
    case SimulationState::SyncingFromGPU:
        handleSyncingState();
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
        syncDataFromGPU();
    }
}

void ProvinceSimulationGPU::handleSyncingState() {
    if (!material_) {
        state_ = SimulationState::Idle;
        return;
    }

    material_->PollCompletedTasks();

    if (material_->AreTasksComplete(activeSyncTasks_)) {
        updateCachedData();
        activeSyncTasks_.clear();
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

void ProvinceSimulationGPU::syncDataFromGPU() {
    if (!material_) return;

    activeSyncTasks_ = material_->SyncFromGPUAsync();

    if (activeSyncTasks_.empty()) {
        state_ = SimulationState::Idle;
        return;
    }

    state_ = SimulationState::SyncingFromGPU;
}

void ProvinceSimulationGPU::updateCachedData() {
    if (!material_) return;

    auto inputOutputBuffer = material_->GetInputOutputBuffer();
    if (!inputOutputBuffer) {
        SPDLOG_ERROR("GPU: InputOutput buffer not available during cache update");
        return;
    }

    const uint8_t* gpuBuffer = inputOutputBuffer->GetRawBuffer();
    const size_t bufferSize = sizeof(ProvinceDataBuffer);

    // Asynchronous copy from GPU buffer to working buffer
    auto copyFutures = asyncMemOps_->Memcpy(
        workingBuffer_.get(),
        gpuBuffer,
        bufferSize
    );

    // Wait for copy completion
    AsyncMemoryOps::WaitForAll(copyFutures);

    SPDLOG_DEBUG("Updated cached data using async memcpy ({} bytes, {} chunks)",
        bufferSize, copyFutures.size());

    // Update UI cache from working buffer
    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        cachedStats_[i] = workingBuffer_->provinces[i];
    }
}

void ProvinceSimulationGPU::syncParametersToMaterial() {
    if (!material_) return;

    auto inputBuffer = material_->GetInputBuffer();
    if (!inputBuffer) {
        SPDLOG_ERROR("GPU: Input buffer not available");
        return;
    }

    uint8_t* rawBuffer = inputBuffer->GetRawBuffer();

    std::memcpy(rawBuffer, &simParams_, sizeof(SimulationParameters));

    SPDLOG_DEBUG("Synced simulation parameters ({} bytes)", sizeof(SimulationParameters));
}
