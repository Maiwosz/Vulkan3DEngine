#include "Scene.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "TransformComponent.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "LightComponent.h"
#include "CameraComponent.h"
#include "ScriptComponent.h"
#include "Engine.h"
#include "InputSystem.h"
#include <spdlog/spdlog.h>

Scene::Scene(Engine& engine, Registry& registry) :
    m_engine(engine), m_registry(registry)
{

    if (true) {
    // Creating object with mesh
    testEntity = m_registry.entities().create();
    {
        // Konfiguracja zasobów
        std::string meshName = "Flora_C";
        std::string materialName = "Flora_c";

        // Transform with rotation
        auto& transform = m_registry.components().addComponent<TransformComponent>(testEntity);
        transform.setPosition(glm::vec3(0.0f, 2.0f, 0.0f));

        transform.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));

        transform.setScale(glm::vec3(0.4f));

        // Mesh and material
        auto& mesh = m_registry.components().addComponent<MeshComponent>(testEntity);
        mesh.setMesh(AssetHandle(AssetLib::AssetType::Mesh, meshName));

        auto& material = m_registry.components().addComponent<MaterialComponent>(testEntity);
        material.setMaterial(AssetHandle(AssetLib::AssetType::Material, materialName));
    }

    testEntity2 = m_registry.entities().create();
    {
        // Konfiguracja zasobów
        std::string meshName = "Hygieia_C";
        std::string materialName = "Hygieia_C";

        // Transform with rotation
        auto& transform = m_registry.components().addComponent<TransformComponent>(testEntity2);
        transform.setPosition(glm::vec3(5.0f, -0.2f, 0.0f));

        transform.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));

        transform.setScale(glm::vec3(1.0f));

        // Mesh and material
        auto& mesh = m_registry.components().addComponent<MeshComponent>(testEntity2);
        mesh.setMesh(AssetHandle(AssetLib::AssetType::Mesh, meshName));

        auto& material = m_registry.components().addComponent<MaterialComponent>(testEntity2);
        material.setMaterial(AssetHandle(AssetLib::AssetType::Material, materialName));
    }

    testEntity3 = m_registry.entities().create();
    {
        // Konfiguracja zasobów
        std::string meshName = "Omphale_C";
        std::string materialName = "Omphale_C";

        // Transform with rotation
        auto& transform = m_registry.components().addComponent<TransformComponent>(testEntity3);
        transform.setPosition(glm::vec3(-5.0f, 0.0f, 0.0f));

        // Apply rotation - 30 degrees around Y axis
        float rotationAngle = 30.0f;
        glm::vec3 rotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
        transform.setRotation(glm::vec3(0.0f, rotationAngle, 0.0f));

        transform.setScale(glm::vec3(0.3f));

        // Mesh and material
        auto& mesh = m_registry.components().addComponent<MeshComponent>(testEntity3);
        mesh.setMesh(AssetHandle(AssetLib::AssetType::Mesh, meshName));

        auto& material = m_registry.components().addComponent<MaterialComponent>(testEntity3);
        material.setMaterial(AssetHandle(AssetLib::AssetType::Material, materialName));
    }

    // floor
    testFloor = m_registry.entities().create();
    {
        // Konfiguracja zasobów
        std::string meshName = "quad";
        std::string materialName = "floor";

        // Transform with rotation
        auto& transform = m_registry.components().addComponent<TransformComponent>(testFloor);
        transform.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));

        transform.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));

        transform.setScale(glm::vec3(1.0f));

        // Mesh and material
        auto& mesh = m_registry.components().addComponent<MeshComponent>(testFloor);
        mesh.setMesh(AssetHandle(AssetLib::AssetType::Mesh, meshName));

        auto& material = m_registry.components().addComponent<MaterialComponent>(testFloor);
        material.setMaterial(AssetHandle(AssetLib::AssetType::Material, materialName));
    }

    // Camera with script
    testCamera = m_registry.entities().create();
    {
        auto& transform = m_registry.components().addComponent<TransformComponent>(testCamera);

        // Initial camera position (will be overridden by script)
        transform.setPosition(glm::vec3(0.0f, 2.0f, 15.0f));
        transform.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));

        // Add camera component
        auto& camera = m_registry.components().addComponent<CameraComponent>(testCamera);
        float fieldOfView = 45.0f;
        float aspectRatio = 1920.0f / 1080.0f;
        float nearPlane = 0.1f;
        float farPlane = 100.0f;
        camera.setVerticalFOV(fieldOfView);
        camera.setAspectRatio(aspectRatio);
        camera.setClippingPlanes(nearPlane, farPlane);

        // Add light orbiter script
        auto& script = m_registry.components().addComponent<ScriptComponent>(testCamera);
        script.setScript("CameraController");
    }

    // Directional light
    testDirectionalLight = m_registry.entities().create();
    {
        auto& transform = m_registry.components().addComponent<TransformComponent>(testDirectionalLight);
        auto& light = m_registry.components().addComponent<LightComponent>(testDirectionalLight, LightComponent::Type::Directional);

        glm::vec3 directionalLightDir = glm::vec3(1.0f, -1.0f, 1.0f);
        glm::vec4 directionalLightColor = glm::vec4(1.0f, 0.8f, 0.8f, 0.005f);

        light.setDirection(directionalLightDir);
        light.setColor(directionalLightColor);
        light.setIntensity(directionalLightColor.w);
    }

    //Point light with orbit script
   // testPointLight = m_registry.create("testPointLight");
   // {
   //     auto& transform = m_registry.addComponent<TransformComponent>(testPointLight);
   //
   //     // Initial position (will be overridden by script)
   //     transform.setPosition(glm::vec3(7.0f, 3.0f, 0.0f));
   //
   //     // Add light component
   //     auto& light = m_registry.addComponent<LightComponent>(testPointLight, LightComponent::Type::Point);
   //     light.setRadius(12.0f);
   //     light.setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
   //
   //     // Add light orbiter script
   //     auto& script = m_registry.addComponent<ScriptComponent>(testPointLight);
   //     script.setScript("LightOrbiter");
   // }
   //
   // testPointLight2 = m_registry.create();
   // {
   //     auto& transform = m_registry.addComponent<TransformComponent>(testPointLight2);
   //
   //	m_registry.setParent(testPointLight2, testPointLight);
   //
   //     // Initial position (will be overridden by script)
   //     transform.setLocalPosition(glm::vec3(20.0f, 0.0f, 0.0f));
   //
   //     // Add light component
   //     auto& light = m_registry.addComponent<LightComponent>(testPointLight2, LightComponent::Type::Point);
   //     light.setRadius(12.0f);
   //     light.setColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
   // }
   //PrefabManager& prefabManager = Engine::get().assetSystem().prefabManager();
   //PrefabHandle handle = m_registry.prefabs().createPrefabFromEntity(testPointLight);
   //prefabManager.savePrefabToFile(handle);



    AssetHandle handle = AssetHandle(AssetLib::AssetType::Prefab, "testPointLight");
    m_engine.assetSystem().assetManager().ensureReady(handle);
    PrefabManager& prefabManager = m_engine.assetSystem().prefabManager();
    PrefabHandle prefabHandle = prefabManager.getHandle<PrefabHandle>(handle.filename);
    m_registry.prefabs().createInstance(prefabHandle);

    m_registry.scenes().saveScene("testScene");
    }

    //AssetHandle handle = AssetHandle(AssetLib::AssetType::Scene, "testScene");
    //m_engine.assetSystem().assetManager().ensureReady(handle);
    m_registry.scenes().loadScene("testScene");
}

Scene::~Scene()
{

}

void Scene::update()
{
    m_registry.systems().updateAll();
    static bool firstRun = true;
    if (firstRun) {
        firstRun = false;
        testComputeShader();
    }
}

void Scene::testComputeShader()
{
    try {
        SPDLOG_WARN("=== Starting Compute Shader Test ===");

        MaterialManager& materialManager = m_engine.assetSystem().materialManager();
        ComputeDispatcher& computeDispatcher = m_engine.engineCore().renderer().computeDispatcher();

        // Create compute material
        auto material = materialManager.createComputeMaterial("ComputeTest");
        if (!material) {
            SPDLOG_ERROR("Failed to create compute material");
            return;
        }

        SPDLOG_WARN("Compute material created: {}", material->GetName());

        // Verify 'values' field exists
        if (!material->HasField("values")) {
            SPDLOG_ERROR("Material doesn't have 'values' field");
            SPDLOG_WARN("Available fields:");
            for (const auto& fieldName : material->GetFieldNames()) {
                SPDLOG_WARN("  - {}", fieldName);
            }
            return;
        }

        // Get array size
        size_t dataSize = material->GetArraySize("values");
        if (dataSize == 0) {
            SPDLOG_ERROR("'values' is not an array");
            return;
        }

        SPDLOG_WARN("Array size: {} elements", dataSize);

        // Fill array with test data: 0.0, 1.0, 2.0, ...
        SPDLOG_WARN("Filling array with test data...");
        auto values = (*material)["values"];
        for (uint32_t i = 0; i < dataSize; ++i) {
            values[i] = static_cast<float>(i);
        }

        // Sync to GPU
        SPDLOG_WARN("Syncing data to GPU...");
        material->SyncToGPU();

        // Dispatch compute shader
        SPDLOG_WARN("Dispatching compute shader for {} elements...", dataSize);
        if (!computeDispatcher.dispatchForDataSize(material, dataSize, 1, 1)) {
            SPDLOG_ERROR("Compute dispatch failed");
            return;
        }

        SPDLOG_WARN("Compute shader executed successfully");

        // Sync results from GPU
        SPDLOG_WARN("Syncing results from GPU...");
        material->SyncFromGPU();

        // Verify results
        SPDLOG_WARN("Verifying results (expected: each value doubled)...");
        SPDLOG_WARN("Output data (first 10 values):");

        bool allCorrect = true;
        for (uint32_t i = 0; i < std::min(10u, static_cast<uint32_t>(dataSize)); ++i) {
            float outputValue = values[i];
            float expected = static_cast<float>(i) * 2.0f;
            bool correct = std::abs(outputValue - expected) < 0.001f;
            allCorrect &= correct;

            SPDLOG_WARN("  [{}] Output: {:.1f}, Expected: {:.1f} {}",
                i, outputValue, expected, correct ? "✓" : "✗");
        }

        // Check remaining values
        for (uint32_t i = 10; i < dataSize; ++i) {
            float outputValue = values[i];
            float expected = static_cast<float>(i) * 2.0f;
            if (std::abs(outputValue - expected) >= 0.001f) {
                allCorrect = false;
                SPDLOG_ERROR("  [{}] Mismatch! Output: {:.1f}, Expected: {:.1f}",
                    i, outputValue, expected);
            }
        }

        if (allCorrect) {
            SPDLOG_WARN("=== TEST PASSED: All {} values correctly doubled ===", dataSize);
        }
        else {
            SPDLOG_ERROR("=== TEST FAILED: Some values incorrect ===");
        }

        SPDLOG_WARN("=== Compute Shader Test Complete ===");
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in testComputeShader: {}", e.what());
        SPDLOG_WARN("=== Compute Shader Test Failed ===");
    }
}
