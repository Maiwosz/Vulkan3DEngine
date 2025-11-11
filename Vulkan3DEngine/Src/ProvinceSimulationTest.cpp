#include "ProvinceSimulationTest.h"
#include <random>

const char* ProvinceSimulationTest::getScriptName() const {
    return "ProvinceSimulationTest";
}

void ProvinceSimulationTest::OnCreate() {
    SPDLOG_INFO("ProvinceSimulationTest created for entity {}", entity.id);

    // Cache dispatcher reference
    auto* engine = getEngine();
    if (engine) {
        computeDispatcher_ = &engine->engineCore().renderer().computeDispatcher();
    }

    initializeSimulation();
}

void ProvinceSimulationTest::OnUpdate(float deltaTime) {
    if (!simulationRunning_ || !material_ || !computeDispatcher_) return;

    //Tymczasowe rozwiązanie, trzeba poprawić obsługę zależności runtime assetów
    AssetManager& assetManager = getEngine()->assetSystem().assetManager();
    assetManager.ensureReady(AssetHandle(AssetLib::AssetType::Shader, "ProvinceSimulation"));

    updateStateMachine();
}

void ProvinceSimulationTest::OnDestroy() {
    SPDLOG_INFO("ProvinceSimulationTest destroyed for entity {}", entity.id);

    // Wait for all pending operations
    if (computeDispatcher_) {
        if (activeComputeTask_) {
            computeDispatcher_->waitForTask(*activeComputeTask_);
        }
        if (material_ && !activeSyncTasks_.empty()) {
            material_->WaitForTasks(activeSyncTasks_);
        }
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool ProvinceSimulationTest::isComputeInProgress() const {
    if (!computeDispatcher_ || !activeComputeTask_) return false;
    return !computeDispatcher_->isTaskComplete(*activeComputeTask_);
}

ProvinceStats ProvinceSimulationTest::getProvinceData(uint32_t index) const {
    if (index >= cachedStats_.size()) {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    return cachedStats_[index];
}

ProvinceStats ProvinceSimulationTest::getInitialStats(uint32_t index) const {
    if (index >= initialStats_.size()) {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    return initialStats_[index];
}

void ProvinceSimulationTest::setSimulationParameters(const SimulationParameters& params) {
    if (state_ != SimulationState::Idle) {
        SPDLOG_WARN("Cannot change simulation parameters while operation is in progress");
        return;
    }

    simParams_ = params;
    syncParametersToMaterial();

    SPDLOG_INFO("Simulation parameters updated");
}

void ProvinceSimulationTest::setRandomizationParameters(const RandomizationParameters& params) {
    if (state_ != SimulationState::Idle) {
        SPDLOG_WARN("Cannot change randomization parameters while operation is in progress");
        return;
    }

    randParams_ = params;
    SPDLOG_INFO("Randomization parameters updated");
}

void ProvinceSimulationTest::runSingleStep() {
    if (!simulationRunning_ || !material_ || !computeDispatcher_) {
        SPDLOG_WARN("Cannot run step: simulation not ready");
        return;
    }

    if (state_ != SimulationState::Idle && state_ != SimulationState::ReadyForNextStep) {
        SPDLOG_WARN("Cannot run step: operation already in progress (state: {})", (int)state_);
        return;
    }

    pendingSteps_ = 0;
    dispatchNextStep();
}

void ProvinceSimulationTest::runMultipleSteps(uint32_t numSteps) {
    if (!simulationRunning_ || !material_ || !computeDispatcher_ || numSteps == 0) {
        SPDLOG_WARN("Cannot run steps: simulation not ready or numSteps == 0");
        return;
    }

    if (state_ != SimulationState::Idle && state_ != SimulationState::ReadyForNextStep) {
        SPDLOG_WARN("Cannot run steps: operation already in progress");
        return;
    }

    // Queue remaining steps, dispatch first immediately
    pendingSteps_ = numSteps - 1;
    dispatchNextStep();
}

void ProvinceSimulationTest::resetSimulation() {
    resetSimulationWithParameters(simParams_, randParams_);
}

void ProvinceSimulationTest::resetSimulationWithParameters(const SimulationParameters& simParams,
    const RandomizationParameters& randParams) {
    // Wait for all pending operations
    if (computeDispatcher_ && activeComputeTask_) {
        computeDispatcher_->waitForTask(*activeComputeTask_);
        activeComputeTask_.reset();
    }

    if (material_ && !activeSyncTasks_.empty()) {
        material_->WaitForTasks(activeSyncTasks_);
        activeSyncTasks_.clear();
    }

    stepCounter_ = 0;
    pendingSteps_ = 0;
    state_ = SimulationState::Idle;

    // Update parameters
    simParams_ = simParams;
    randParams_ = randParams;

    initializeSimulation();

    SPDLOG_INFO("Simulation reset complete with new parameters");
}

void ProvinceSimulationTest::requestDataRefresh() {
    if (state_ != SimulationState::ReadyForNextStep && state_ != SimulationState::Idle) {
        SPDLOG_WARN("Cannot refresh data: operation in progress");
        return;
    }

    syncDataFromGPU();
}

bool ProvinceSimulationTest::isDataRefreshComplete() const {
    if (activeSyncTasks_.empty()) return true;
    if (!material_) return false;

    return material_->AreTasksComplete(activeSyncTasks_);
}

// ============================================================================
// STATE MACHINE
// ============================================================================

void ProvinceSimulationTest::updateStateMachine() {
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
        // Nothing to do
        break;
    }
}

void ProvinceSimulationTest::handleComputingState() {
    if (!activeComputeTask_ || !computeDispatcher_) {
        state_ = SimulationState::Idle;
        return;
    }

    // Check if compute finished
    if (computeDispatcher_->isTaskComplete(*activeComputeTask_)) {
        SPDLOG_DEBUG("Compute step {} completed", stepCounter_);

        activeComputeTask_.reset();
        stepCounter_++;

        // Start syncing data from GPU
        syncDataFromGPU();
    }
}

void ProvinceSimulationTest::handleSyncingState() {
    if (!material_) {
        state_ = SimulationState::Idle;
        return;
    }

    // Poll for completed sync tasks
    material_->PollCompletedTasks();

    // Check if all sync tasks completed
    if (material_->AreTasksComplete(activeSyncTasks_)) {
        SPDLOG_DEBUG("GPU sync completed for step {}", stepCounter_);

        // Update cached data
        updateCachedData();

        // Clear completed tasks
        activeSyncTasks_.clear();

        state_ = SimulationState::ReadyForNextStep;
    }
}

void ProvinceSimulationTest::handleReadyState() {
    // If we have pending steps, dispatch next immediately
    if (pendingSteps_ > 0) {
        pendingSteps_--;
        dispatchNextStep();
    }
    else {
        state_ = SimulationState::Idle;
    }
}

// ============================================================================
// COMPUTE & SYNC
// ============================================================================

bool ProvinceSimulationTest::dispatchNextStep() {
    if (!computeDispatcher_ || !material_) return false;

    // Dispatch compute (async, returns immediately)
    ComputeTaskHandle task = computeDispatcher_->dispatchForDataSize(
        material_,
        simParams_.numProvinces,
        1,
        1
    );

    if (!task) {
        SPDLOG_ERROR("Failed to dispatch compute task");
        state_ = SimulationState::Idle;
        return false;
    }

    activeComputeTask_ = task;
    state_ = SimulationState::Computing;

    SPDLOG_DEBUG("Dispatched compute step {}", stepCounter_ + 1);
    return true;
}

void ProvinceSimulationTest::syncDataFromGPU() {
    if (!material_) return;

    // Start async sync from GPU (non-blocking)
    activeSyncTasks_ = material_->SyncFromGPUAsync();

    if (activeSyncTasks_.empty()) {
        SPDLOG_WARN("No sync tasks created");
        state_ = SimulationState::Idle;
        return;
    }

    state_ = SimulationState::SyncingFromGPU;
    SPDLOG_DEBUG("Started GPU sync with {} tasks", activeSyncTasks_.size());
}

void ProvinceSimulationTest::updateCachedData() {
    if (!material_) return;

    // At this point, CPU buffers are guaranteed to be up-to-date
    // Extract data into cache
    auto provinces = (*material_)["provinces"];

    cachedStats_.resize(simParams_.numProvinces);

    for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
        auto province = provinces[i];

        cachedStats_[i].population = province["population"];
        cachedStats_[i].foodProductionModifier = province["foodProductionModifier"];
        cachedStats_[i].wealth = province["wealth"];
        cachedStats_[i].foodStorage = province["foodStorage"];
    }

    SPDLOG_DEBUG("Updated cached data for {} provinces", simParams_.numProvinces);
}

void ProvinceSimulationTest::syncParametersToMaterial() {
    if (!material_) return;

    (*material_)["numProvinces"] = simParams_.numProvinces;
    (*material_)["foodConsumptionPerPop"] = simParams_.foodConsumptionPerPop;
    (*material_)["basePopulationGrowth"] = simParams_.basePopulationGrowth;
    (*material_)["starvationThreshold"] = simParams_.starvationThreshold;
    (*material_)["wealthPerPop"] = simParams_.wealthPerPop;
    (*material_)["maxFoodStorage"] = simParams_.maxFoodStorage;
    (*material_)["minPopulation"] = simParams_.minPopulation;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ProvinceSimulationTest::initializeSimulation() {
    try {
        SPDLOG_INFO("Initializing Province Simulation");

        auto* engine = getEngine();
        if (!engine) {
            SPDLOG_ERROR("Engine not available");
            return;
        }

        MaterialManager& materialManager = engine->assetSystem().materialManager();

        material_ = materialManager.createComputeMaterial("ProvinceSimulation");
        if (!material_) {
            SPDLOG_ERROR("Failed to create compute material");
            return;
        }

        if (!material_->HasField("provinces")) {
            SPDLOG_ERROR("Material doesn't have 'provinces' field");
            return;
        }

        // Sync simulation parameters to material
        syncParametersToMaterial();

        auto provinces = (*material_)["provinces"];

        SPDLOG_INFO("Initializing {} provinces", simParams_.numProvinces);

        // Initialize province data with randomization parameters
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

        initialStats_.resize(simParams_.numProvinces);
        cachedStats_.resize(simParams_.numProvinces);

        for (uint32_t i = 0; i < simParams_.numProvinces; ++i) {
            auto province = provinces[i];

            float population = popDist(gen);
            float foodProd = foodProdDist(gen);

            province["population"] = population;
            province["foodProductionModifier"] = foodProd;
            province["wealth"] = 0.0f;
            province["foodStorage"] = randParams_.initialFoodStorage;

            // Store initial state
            initialStats_[i] = { population, foodProd, 0.0f, randParams_.initialFoodStorage };

            // Initialize cache
            cachedStats_[i] = initialStats_[i];
        }

        // Zero out unused provinces
        for (uint32_t i = simParams_.numProvinces; i < 1024; ++i) {
            auto province = provinces[i];
            province["population"] = 0.0f;
            province["foodProductionModifier"] = 0.0f;
            province["wealth"] = 0.0f;
            province["foodStorage"] = 0.0f;
        }

        // Sync initial data to GPU (blocking, but only happens once)
        material_->SyncToGPU();

        simulationRunning_ = true;
        pendingSteps_ = 0;
        state_ = SimulationState::Idle;

        SPDLOG_INFO("Simulation initialized and ready (seed: {})",
            randParams_.randomSeed == 0 ? "random" : std::to_string(randParams_.randomSeed));
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in initializeSimulation: {}", e.what());
    }
}
