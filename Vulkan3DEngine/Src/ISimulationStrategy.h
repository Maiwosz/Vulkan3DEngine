#pragma once
#include <cstdint>
#include <atomic>
#include <memory>

// Shared data structures
struct ProvinceData {
    float population;
    float foodProductionModifier;
    float wealth;
    float foodStorage;
};
static_assert(sizeof(ProvinceData) == 16, "ProvinceData must be 16 bytes");

struct SimulationParameters {
    uint32_t numProvinces = 65536;
    float foodConsumptionPerPop = 0.1f;
    float basePopulationGrowth = 0.02f;
    float starvationThreshold = 0.5f;
    float wealthPerPop = 0.5f;
    float maxFoodStorage = 100.0f;
    float minPopulation = 0.1f;
};

struct RandomizationParameters {
    float minPopulation = 5.0f;
    float maxPopulation = 50.0f;
    float minFoodProduction = 1.0f;
    float maxFoodProduction = 10.0f;
    float initialFoodStorage = 10.0f;
    uint32_t randomSeed = 0;
};

struct StepTimings {
    double computeMs = 0.0;
    double readbackMs = 0.0;
    double totalMs = 0.0;
};

constexpr size_t MAX_PROVINCES = 1048576;
struct ProvinceDataBuffer {
    ProvinceData provinces[MAX_PROVINCES];
};

enum class SimulationMode {
    GPU,
    CPU
};

/**
 * Strategy Interface
 *
 * Each strategy runs on dedicated thread and uses BLOCKING operations.
 * This allows for accurate timing and simple control flow:
 *
 * executeSingleStep() {
 *     startTimer();
 *     doWork();          // BLOCKS until complete
 *     stopTimer();
 *     incrementTick();
 * }
 */
class ISimulationStrategy {
public:
    virtual ~ISimulationStrategy() = default;

    // Lifecycle
    virtual bool initialize(
        const SimulationParameters& simParams,
        const RandomizationParameters& randParams,
        ProvinceDataBuffer* sharedBuffer
    ) = 0;

    virtual void shutdown() = 0;

    // Execution - BLOCKING, returns when step completes
    virtual void executeSingleStep() = 0;

    // State queries (thread-safe)
    virtual uint32_t getCurrentTick() const = 0;
    virtual StepTimings getLastStepTimings() const = 0;

    virtual void setAutoReadback(bool enabled) = 0;
    virtual bool isAutoReadback() const = 0;
    virtual void manualReadback() = 0;  // For manual readback trigger

    // Type info
    virtual const char* getTypeName() const = 0;
};
