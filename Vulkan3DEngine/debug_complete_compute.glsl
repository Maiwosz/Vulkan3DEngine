#version 450

struct AggregateData {
    uint totalPopulation;
    uint totalWealth;
    int avgGrowthScaled;
    uint growing;
    uint stable;
    uint declining;
};

struct ProvinceData {
    uint population;
    float foodProductionModifier;
    uint wealth;
    uint foodStorage;
};

layout(std140, set = 2, binding = 0) uniform InputData {
    uint numProvinces;
    float foodConsumptionPerPop;
    float basePopulationGrowth;
    float starvationThreshold;
    float wealthPerPop;
    uint maxFoodStorage;
    uint minPopulation;
    uint enableGPUAggregation;
} inputData;
layout(std430, set = 2, binding = 1) buffer InputOutputData {
    ProvinceData provinces[1048576];
} inputOutputData;
layout(std430, set = 2, binding = 2) writeonly buffer OutputData {
    writeonly AggregateData aggregate;
} outputData;

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

shared uint sharedPopulation[256];
shared uint sharedWealth[256];
shared int sharedGrowth[256];
shared uint sharedGrowing[256];
shared uint sharedStable[256];
shared uint sharedDeclining[256];

void main() {
    uint idx = gl_GlobalInvocationID.x;
    uint localIdx = gl_LocalInvocationID.x;
    
    float FOOD_CONSUMPTION_PER_POP = inputData.foodConsumptionPerPop;
    float BASE_POPULATION_GROWTH = inputData.basePopulationGrowth;
    float STARVATION_THRESHOLD = inputData.starvationThreshold;
    float WEALTH_PER_POP = inputData.wealthPerPop;
    uint MAX_FOOD_STORAGE = inputData.maxFoodStorage;
    uint MIN_POPULATION = inputData.minPopulation;
    
    uint localPopulation = 0;
    uint localWealth = 0;
    int localGrowth = 0;
    uint localGrowing = 0;
    uint localStable = 0;
    uint localDeclining = 0;
    
    if (idx < inputData.numProvinces) {
        ProvinceData province = inputOutputData.provinces[idx];
        uint initialPopulation = province.population;
        
        if (province.population < MIN_POPULATION) {
            province.population = MIN_POPULATION;
            province.foodStorage = 0;
            province.wealth = 0;
            inputOutputData.provinces[idx] = province;
            
            localPopulation = MIN_POPULATION;
            localWealth = 0;
            localGrowth = 0;
            localStable = 1;
        } else {
            // === FOOD PRODUCTION ===
            float foodProduced = province.foodProductionModifier;
            
            // Add to storage - convert float to uint carefully
            uint foodToAdd = uint(floor(foodProduced));
            province.foodStorage = min(province.foodStorage + foodToAdd, MAX_FOOD_STORAGE);
            
            // === FOOD CONSUMPTION ===
            float foodNeeded = float(province.population) * FOOD_CONSUMPTION_PER_POP;
            float foodConsumed = min(float(province.foodStorage), foodNeeded);
            province.foodStorage -= uint(floor(foodConsumed));
            
            float foodRatio = foodConsumed / max(foodNeeded, 0.001);
            
            // === POPULATION DYNAMICS ===
            float populationChangeFloat = 0.0;
            
            if (foodRatio >= 1.0) {
                float excessFood = foodConsumed - foodNeeded;
                float growthBonus = min(excessFood * 0.01, 0.01);
                populationChangeFloat = float(province.population) * (BASE_POPULATION_GROWTH + growthBonus);
            } else if (foodRatio >= STARVATION_THRESHOLD) {
                float partialGrowth = (foodRatio - STARVATION_THRESHOLD) / (1.0 - STARVATION_THRESHOLD);
                populationChangeFloat = float(province.population) * BASE_POPULATION_GROWTH * partialGrowth;
            } else {
                float starvationSeverity = 1.0 - (foodRatio / STARVATION_THRESHOLD);
                populationChangeFloat = -float(province.population) * 0.05 * starvationSeverity;
            }
            
            // Apply population change (convert to int)
            int populationChange = int(round(populationChangeFloat));
            int newPopulation = int(province.population) + populationChange;
            province.population = uint(max(newPopulation, int(MIN_POPULATION)));
            
            // === WEALTH GENERATION ===
            if (foodRatio >= STARVATION_THRESHOLD) {
                float wealthGenerated = float(province.population) * WEALTH_PER_POP * foodRatio;
                province.wealth += uint(floor(wealthGenerated));
            }
            
            inputOutputData.provinces[idx] = province;
            
            // === AGGREGATE STATISTICS ===
            localPopulation = province.population;
            localWealth = province.wealth;
            
            if (initialPopulation > 0) {
                float growthPercent = ((float(province.population) - float(initialPopulation)) / float(initialPopulation)) * 100.0;
                localGrowth = int(round(growthPercent * 100.0));  // Fixed: explicit int cast
                
                if (growthPercent > 0.1) {
                    localGrowing = 1;
                } else if (growthPercent < -0.1) {
                    localDeclining = 1;
                } else {
                    localStable = 1;
                }
            } else {
                localStable = 1;
            }
        }
    }
    
    // === WORKGROUP-LEVEL REDUCTION (tylko jeśli włączone) ===
    //if (inputData.enableGPUAggregation != 0) {
        sharedPopulation[localIdx] = localPopulation;
        sharedWealth[localIdx] = localWealth;
        sharedGrowth[localIdx] = localGrowth;
        sharedGrowing[localIdx] = localGrowing;
        sharedStable[localIdx] = localStable;
        sharedDeclining[localIdx] = localDeclining;
        
        barrier();
        
        for (uint stride = 128; stride > 0; stride >>= 1) {
            if (localIdx < stride) {
                sharedPopulation[localIdx] += sharedPopulation[localIdx + stride];
                sharedWealth[localIdx] += sharedWealth[localIdx + stride];
                sharedGrowth[localIdx] += sharedGrowth[localIdx + stride];
                sharedGrowing[localIdx] += sharedGrowing[localIdx + stride];
                sharedStable[localIdx] += sharedStable[localIdx + stride];
                sharedDeclining[localIdx] += sharedDeclining[localIdx + stride];
            }
            barrier();
        }
        
        if (localIdx == 0) {
            atomicAdd(outputData.aggregate.totalPopulation, sharedPopulation[0]);
            atomicAdd(outputData.aggregate.totalWealth, sharedWealth[0]);
            atomicAdd(outputData.aggregate.avgGrowthScaled, sharedGrowth[0]);
            atomicAdd(outputData.aggregate.growing, sharedGrowing[0]);
            atomicAdd(outputData.aggregate.stable, sharedStable[0]);
            atomicAdd(outputData.aggregate.declining, sharedDeclining[0]);
        }
    //}
}