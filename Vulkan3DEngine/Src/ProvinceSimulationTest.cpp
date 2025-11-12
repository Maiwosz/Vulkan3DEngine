#include "ProvinceSimulationTest.h"

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

    // Create AsyncMemoryOps with thread pool
    if (threadPool_) {
        asyncMemoryOps_ = std::make_unique<AsyncMemoryOps>(*threadPool_);
        SPDLOG_INFO("AsyncMemoryOps initialized with {} threads", threadPool_->getThreadCount());
    }
    else {
        SPDLOG_WARN("ThreadPool not available, AsyncMemoryOps not initialized");
    }

    initializeSimulation();
}

void ProvinceSimulationTest::OnUpdate(float deltaTime) {
    if (!simulation_) return;

    // GPU needs update call for state machine
    if (currentMode_ == SimulationMode::GPU) {
        auto* gpuSim = static_cast<ProvinceSimulationGPU*>(simulation_.get());
        gpuSim->update();
    }

    // Handle asset loading for GPU
    if (currentMode_ == SimulationMode::GPU) {
        AssetManager& assetManager = getEngine()->assetSystem().assetManager();
        assetManager.ensureReady(AssetHandle(AssetLib::AssetType::Shader, "ProvinceSimulation"));
    }
}

void ProvinceSimulationTest::OnDestroy() {
    SPDLOG_INFO("ProvinceSimulationTest destroyed for entity {}", entity.id);
    simulation_.reset();
    asyncMemoryOps_.reset();
}

void ProvinceSimulationTest::setMode(SimulationMode mode) {
    if (mode == currentMode_) return;

    if (simulation_ && simulation_->isComputeInProgress()) {
        SPDLOG_WARN("Cannot switch mode during computation");
        return;
    }

    SPDLOG_INFO("Switching simulation mode: {} -> {}",
        currentMode_ == SimulationMode::GPU ? "GPU" : "CPU",
        mode == SimulationMode::GPU ? "GPU" : "CPU");

    currentMode_ = mode;
    initializeSimulation();
}

void ProvinceSimulationTest::setCPUThreadCount(size_t threads) {
    if (currentMode_ == SimulationMode::CPU) {
        auto* cpuSim = static_cast<ProvinceSimulationCPU*>(simulation_.get());
        if (cpuSim) {
            cpuSim->setThreadCount(threads);
            SPDLOG_INFO("CPU thread count set to {}", threads);
        }
    }
}

size_t ProvinceSimulationTest::getCPUThreadCount() const {
    if (currentMode_ == SimulationMode::CPU && simulation_) {
        auto* cpuSim = static_cast<ProvinceSimulationCPU*>(simulation_.get());
        return cpuSim->getThreadCount();
    }
    return 0;
}

void ProvinceSimulationTest::setSimulationParameters(const SimulationParameters& params) {
    simParams_ = params;
    if (simulation_) {
        simulation_->setSimulationParameters(params);
    }
}

void ProvinceSimulationTest::setRandomizationParameters(const RandomizationParameters& params) {
    randParams_ = params;
    if (simulation_) {
        simulation_->setRandomizationParameters(params);
    }
}

void ProvinceSimulationTest::resetSimulation() {
    if (simulation_) {
        simulation_->reset();
    }
}

void ProvinceSimulationTest::resetSimulationWithParameters(const SimulationParameters& simParams,
    const RandomizationParameters& randParams) {
    simParams_ = simParams;
    randParams_ = randParams;

    if (simulation_) {
        simulation_->setSimulationParameters(simParams_);
        simulation_->setRandomizationParameters(randParams_);
        simulation_->reset();
    }
}

void ProvinceSimulationTest::initializeSimulation() {
    simulation_.reset();

    switch (currentMode_) {
    case SimulationMode::GPU:
        createGPUSimulation();
        break;
    case SimulationMode::CPU:
        createCPUSimulation();
        break;
    }

    if (simulation_) {
        simulation_->initialize(simParams_, randParams_);
    }
}

void ProvinceSimulationTest::createGPUSimulation() {
    if (!computeDispatcher_ || !materialManager_) {
        SPDLOG_ERROR("GPU resources not available");
        return;
    }

    // Create GPU simulation with AsyncMemoryOps
    auto gpuSim = std::make_unique<ProvinceSimulationGPU>(
        computeDispatcher_,
        asyncMemoryOps_.get()
    );

    // Create material
    MaterialSmartHandle material = materialManager_->createComputeMaterial("ProvinceSimulation");
    if (!material) {
        SPDLOG_ERROR("Failed to create GPU material");
        return;
    }

    // Set material before moving ownership
    gpuSim->setMaterial(material);
    simulation_ = std::move(gpuSim);

    SPDLOG_INFO("GPU simulation created with AsyncMemoryOps");
}

void ProvinceSimulationTest::createCPUSimulation() {
    if (!threadPool_) {
        SPDLOG_ERROR("ThreadPool not available");
        return;
    }

    // Create CPU simulation with AsyncMemoryOps
    simulation_ = std::make_unique<ProvinceSimulationCPU>(
        threadPool_,
        asyncMemoryOps_.get()
    );

    SPDLOG_INFO("CPU simulation created with AsyncMemoryOps");
}
