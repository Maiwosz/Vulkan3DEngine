#pragma once
#include "CppScriptBase.h"
#include <spdlog/spdlog.h>
#include "MaterialManager.h"
#include "ComputeDispatcher.h"
#include "Engine.h"

class ComputeShaderTestScript : public CppScriptBase {
public:
    const char* getScriptName() const override {
        return "ComputeShaderTestScript";
    }

    void OnCreate() override {
        SPDLOG_INFO("ComputeShaderTestScript created for entity {}", entity.id);
        runComputeShaderTest();
    }

    void OnUpdate(float deltaTime) override {
        // Test runs only once in OnCreate
    }

    void OnDestroy() override {
        SPDLOG_INFO("ComputeShaderTestScript destroyed for entity {}", entity.id);
    }

private:
    void runComputeShaderTest() {
        try {
            //SPDLOG_WARN("=== Starting Compute Shader Test ===");

            //auto* registry = getRegistry();
            //if (!registry) {
            //    SPDLOG_ERROR("Registry not available");
            //    return;
            //}

            //auto* engine = getEngine();
            //if (!engine) {
            //    SPDLOG_ERROR("Engine not available");
            //    return;
            //}

            //MaterialManager& materialManager = engine->assetSystem().materialManager();
            //ComputeDispatcher& computeDispatcher = engine->engineCore().renderer().computeDispatcher();

            //// Create compute material
            //auto material = materialManager.createComputeMaterial("ComputeTest");
            //if (!material) {
            //    SPDLOG_ERROR("Failed to create compute material");
            //    return;
            //}
            //SPDLOG_WARN("Compute material created: {}", material->GetName());

            //// Verify 'values' field exists
            //if (!material->HasField("values")) {
            //    SPDLOG_ERROR("Material doesn't have 'values' field");
            //    SPDLOG_WARN("Available fields:");
            //    for (const auto& fieldName : material->GetFieldNames()) {
            //        SPDLOG_WARN("  - {}", fieldName);
            //    }
            //    return;
            //}

            //// Get array size
            //size_t dataSize = material->GetArraySize("values");
            //if (dataSize == 0) {
            //    SPDLOG_ERROR("'values' is not an array");
            //    return;
            //}
            //SPDLOG_WARN("Array size: {} elements", dataSize);

            //// Fill array with test data: 0.0, 1.0, 2.0, ...
            //SPDLOG_WARN("Filling array with test data...");
            //auto values = (*material)["values"];
            //for (uint32_t i = 0; i < dataSize; ++i) {
            //    values[i] = static_cast<float>(i);
            //}

            //// Sync to GPU
            //SPDLOG_WARN("Syncing data to GPU...");
            //material->SyncToGPU();

            //// Dispatch compute shader
            //SPDLOG_WARN("Dispatching compute shader for {} elements...", dataSize);
            //if (!computeDispatcher.dispatchForDataSize(material, dataSize, 1, 1)) {
            //    SPDLOG_ERROR("Compute dispatch failed");
            //    return;
            //}
            //SPDLOG_WARN("Compute shader executed successfully");

            //// Sync results from GPU
            //SPDLOG_WARN("Syncing results from GPU...");
            //material->SyncFromGPU();

            //// Verify results
            //SPDLOG_WARN("Verifying results (expected: each value doubled)...");
            //SPDLOG_WARN("Output data (first 10 values):");
            //bool allCorrect = true;
            //for (uint32_t i = 0; i < std::min(10u, static_cast<uint32_t>(dataSize)); ++i) {
            //    float outputValue = values[i];
            //    float expected = static_cast<float>(i) * 2.0f;
            //    bool correct = std::abs(outputValue - expected) < 0.001f;
            //    allCorrect &= correct;
            //    SPDLOG_WARN("  [{}] Output: {:.1f}, Expected: {:.1f} {}",
            //        i, outputValue, expected, correct ? "✓" : "✗");
            //}

            //// Check remaining values
            //for (uint32_t i = 10; i < dataSize; ++i) {
            //    float outputValue = values[i];
            //    float expected = static_cast<float>(i) * 2.0f;
            //    if (std::abs(outputValue - expected) >= 0.001f) {
            //        allCorrect = false;
            //        SPDLOG_ERROR("  [{}] Mismatch! Output: {:.1f}, Expected: {:.1f}",
            //            i, outputValue, expected);
            //    }
            //}

            //if (allCorrect) {
            //    SPDLOG_WARN("=== TEST PASSED: All {} values correctly doubled ===", dataSize);
            //}
            //else {
            //    SPDLOG_ERROR("=== TEST FAILED: Some values incorrect ===");
            //}

            //SPDLOG_WARN("=== Compute Shader Test Complete ===");
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("Exception in testComputeShader: {}", e.what());
            SPDLOG_WARN("=== Compute Shader Test Failed ===");
        }
    }
};
