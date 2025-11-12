#version 450

struct ProvinceData {
    float population;
    float foodProductionModifier;
    float wealth;
    float foodStorage;
};

layout(std140, set = 2, binding = 0) uniform InputData {
    uint numProvinces;
    float foodConsumptionPerPop;
    float basePopulationGrowth;
    float starvationThreshold;
    float wealthPerPop;
    float maxFoodStorage;
    float minPopulation;
} inputData;
layout(std430, set = 2, binding = 2) buffer InputOutputData {
    ProvinceData provinces[1048576];
} inputOutputData;

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    
    // Early exit if beyond active provinces
    if (idx >= inputData.numProvinces) return;
    
    // Load simulation parameters from uniform buffer
    float FOOD_CONSUMPTION_PER_POP = inputData.foodConsumptionPerPop;
    float BASE_POPULATION_GROWTH = inputData.basePopulationGrowth;
    float STARVATION_THRESHOLD = inputData.starvationThreshold;
    float WEALTH_PER_POP = inputData.wealthPerPop;
    float MAX_FOOD_STORAGE = inputData.maxFoodStorage;
    float MIN_POPULATION = inputData.minPopulation;
    
    // Load province data
    ProvinceData province = inputOutputData.provinces[idx];
    
    // Skip if population is negligible
    if (province.population < MIN_POPULATION) {
        province.population = MIN_POPULATION;
        province.foodStorage = 0.0;
        province.wealth = 0.0;
        inputOutputData.provinces[idx] = province;
        return;
    }
    
    // === FOOD PRODUCTION ===
    float foodProduced = province.foodProductionModifier;
    
    // Add to food storage (capped)
    province.foodStorage += foodProduced;
    province.foodStorage = min(province.foodStorage, MAX_FOOD_STORAGE);
    
    // === FOOD CONSUMPTION ===
    float foodNeeded = province.population * FOOD_CONSUMPTION_PER_POP;
    float foodConsumed = min(province.foodStorage, foodNeeded);
    province.foodStorage -= foodConsumed;
    
    // Calculate food availability ratio
    float foodRatio = foodConsumed / max(foodNeeded, 0.001);
    
    // === POPULATION DYNAMICS ===
    float populationChange = 0.0;
    
    if (foodRatio >= 1.0) {
        // Well-fed: population grows
        float excessFood = foodConsumed - foodNeeded;
        float growthBonus = min(excessFood * 0.01, 0.01); // Bonus growth from excess food
        populationChange = province.population * (BASE_POPULATION_GROWTH + growthBonus);
    } else if (foodRatio >= STARVATION_THRESHOLD) {
        // Moderate food: slow growth or stability
        float partialGrowth = (foodRatio - STARVATION_THRESHOLD) / (1.0 - STARVATION_THRESHOLD);
        populationChange = province.population * BASE_POPULATION_GROWTH * partialGrowth;
    } else {
        // Starvation: population declines
        float starvationSeverity = 1.0 - (foodRatio / STARVATION_THRESHOLD);
        populationChange = -province.population * 0.05 * starvationSeverity;
    }
    
    province.population += populationChange;
    province.population = max(province.population, MIN_POPULATION);
    
    // === WEALTH GENERATION ===
    // Only generate wealth if population is stable/growing and has food
    if (foodRatio >= STARVATION_THRESHOLD) {
        float wealthGenerated = province.population * WEALTH_PER_POP * foodRatio;
        province.wealth += wealthGenerated;
    }
    
    // Write back
    inputOutputData.provinces[idx] = province;
}