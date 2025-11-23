#pragma once
#include <cstdint>
#include <atomic>
#include <memory>

struct ProvinceData {
    uint32_t population;              // Population in thousands (discrete)
    float foodProductionModifier;     // Production multiplier (can be fractional)
    uint32_t wealth;                  // Wealth in units (discrete)
    uint32_t foodStorage;             // Food units stored (discrete)
};
static_assert(sizeof(ProvinceData) == 16, "ProvinceData must be 16 bytes");

// Unified aggregate statistics structure
struct AggregateStatistics {
    uint32_t totalPopulation;
    uint32_t totalWealth;
    float avgGrowth;
    uint32_t growing;
    uint32_t stable;
    uint32_t declining;
};

struct SimulationParameters {
    uint32_t numProvinces = 65536;
    float foodConsumptionPerPop = 0.1f;
    float basePopulationGrowth = 0.02f;
    float starvationThreshold = 0.5f;
    float wealthPerPop = 0.5f;
    uint32_t maxFoodStorage = 100;
    uint32_t minPopulation = 1;
    uint32_t enableGPUAggregation = 1;
};

struct RandomizationParameters {
    uint32_t minPopulation = 5;
    uint32_t maxPopulation = 50;
    float minFoodProduction = 1.0f;
    float maxFoodProduction = 10.0f;
    uint32_t initialFoodStorage = 10;
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
 * Strategy Interface - Unified Aggregation
 *
 * Both CPU and GPU strategies now provide aggregate statistics through
 * a unified interface. GPU computes aggregates in shader, CPU computes
 * them during simulation or readback.
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

    // Unified aggregate statistics interface
    virtual AggregateStatistics getAggregateStatistics() const = 0;

    // Readback control
    virtual void setAutoReadback(bool enabled) = 0;
    virtual bool isAutoReadback() const = 0;
    virtual void manualReadback() = 0;

    // Type info
    virtual const char* getTypeName() const = 0;
};
