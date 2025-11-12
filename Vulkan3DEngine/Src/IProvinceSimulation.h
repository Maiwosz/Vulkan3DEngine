#pragma once
#include <vector>
#include <cstdint>

#pragma once
#include <cstdint>

/**
 * Common structures for both CPU and GPU province simulations
 * These structures are shared between CPU and GPU implementations
 * GPU shaders reference these through ProvinceSimGPU namespace
 */

 // Single province data (16 bytes, std430/natural alignment)
struct ProvinceData {
    float population;
    float foodProductionModifier;
    float wealth;
    float foodStorage;
};
static_assert(sizeof(ProvinceData) == 16, "ProvinceData must be 16 bytes");
static_assert(alignof(ProvinceData) == 4, "ProvinceData must be 4-byte aligned");

// Simulation parameters (28 bytes, std140 compatible)
struct SimulationParameters {
    uint32_t numProvinces = 65536;
    float foodConsumptionPerPop = 0.1f;
    float basePopulationGrowth = 0.02f;
    float starvationThreshold = 0.5f;
    float wealthPerPop = 0.5f;
    float maxFoodStorage = 100.0f;
    float minPopulation = 0.1f;
};
static_assert(sizeof(SimulationParameters) == 28, "SimulationParameters must be 28 bytes");

// Initialization parameters
struct RandomizationParameters {
    float minPopulation = 5.0f;
    float maxPopulation = 50.0f;
    float minFoodProduction = 1.0f;
    float maxFoodProduction = 10.0f;
    float initialFoodStorage = 10.0f;
    uint32_t randomSeed = 0; // 0 = use random_device
};

// Large buffer for all provinces (1 MB)
constexpr size_t MAX_PROVINCES = 65536;
struct ProvinceDataBuffer {
    ProvinceData provinces[MAX_PROVINCES];
};
static_assert(sizeof(ProvinceDataBuffer) == MAX_PROVINCES * 16, "Buffer size must be 1048576 bytes");

/**
 * Common interface for GPU and CPU province simulations
 */
class IProvinceSimulation {
public:
    virtual ~IProvinceSimulation() = default;

    // Initialization
    virtual bool initialize(const SimulationParameters& simParams,
        const RandomizationParameters& randParams) = 0;

    // Simulation control
    virtual void runSingleStep() = 0;
    virtual void runMultipleSteps(uint32_t numSteps) = 0;
    virtual void reset() = 0;

    // Status queries
    virtual bool isComputeInProgress() const = 0;
    virtual uint32_t getCurrentTick() const = 0;

    // Data access
    virtual ProvinceData getProvinceData(uint32_t index) const = 0;
    virtual ProvinceData getInitialStats(uint32_t index) const = 0;

    // Parameters
    virtual const SimulationParameters& getSimulationParameters() const = 0;
    virtual const RandomizationParameters& getRandomizationParameters() const = 0;
    virtual void setSimulationParameters(const SimulationParameters& params) = 0;
    virtual void setRandomizationParameters(const RandomizationParameters& params) = 0;

    // Refresh (for GPU - may be no-op for CPU)
    virtual void requestDataRefresh() = 0;
    virtual bool isDataRefreshComplete() const = 0;

    // Type identification
    virtual const char* getTypeName() const = 0;
};
