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
    testComputeShader();
}

void Scene::testComputeShader()
{
    SPDLOG_INFO("=== Starting Compute Shader Test ===");
    AssetManager& assetManager = m_engine.assetSystem().assetManager();
    ShaderManager& shaderManager = m_engine.assetSystem().shaderManager();
    MaterialManager& materialManager = m_engine.assetSystem().materialManager();
    ComputeDispatcher& computeDispatcher = m_engine.engineCore().renderer().computeDispatcher();

    // Create smart handle for the material
    auto smartMaterial = materialManager.createComputeMaterial("ComputeTest");
    if (!smartMaterial) {
        SPDLOG_ERROR("Failed to create smart material handle");
        return;
    }

    SPDLOG_INFO("Compute material created");

    // Find the InputOutputData buffer in shader metadata
    SPDLOG_INFO("Looking for InputOutputData buffer...");
    const ShaderLib::BufferObject* bufferObject = nullptr;

    ShaderLib::ShaderMetadata metadata = smartMaterial->shader()->metadata;

    for (const auto& buffer : metadata.customBuffers) {
        if (buffer.name == "InputOutputData")
        {
            bufferObject = &buffer;
            SPDLOG_INFO("Found buffer: {} (size: {} bytes)", buffer.name, buffer.size);
            break;
        }
    }

    if (!bufferObject) {
        SPDLOG_ERROR("InputOutputData buffer not found in shader metadata");
        return;
    }

    // Find the 'values' variable in the buffer
    SPDLOG_INFO("Looking for 'values' array variable...");
    const ShaderLib::BufferVariable* valuesVariable = nullptr;
    for (const auto& variable : bufferObject->variables) {
        if (variable.name == "values") {
            valuesVariable = &variable;
            break;
        }
    }

    if (!valuesVariable || !valuesVariable->IsComposite()) {
        SPDLOG_ERROR("'values' array variable not found or is not composite type");
        return;
    }

    SPDLOG_INFO("Found 'values' array variable");

    // Create ShaderArray instance from the variable's composite definition
    SPDLOG_INFO("Creating shader array instance...");
    auto arrayInstance = std::dynamic_pointer_cast<ShaderLib::ShaderArrayInstance>(
        valuesVariable->composite->CreateInstance()
    );

    if (!arrayInstance) {
        SPDLOG_ERROR("Failed to create array instance");
        return;
    }

    const uint32_t DATA_SIZE = arrayInstance->size();
    SPDLOG_INFO("Array instance created (size: {}, element count: {})",
        arrayInstance->GetDefinition()->GetSize(),
        DATA_SIZE);

    // Fill array directly with test data: 0.0, 1.0, 2.0, ..., 255.0
    SPDLOG_INFO("Filling array with test data...");

    // Option 2: Using standard loop with operator[]
    for (uint32_t i = 0; i < DATA_SIZE; ++i) {
        (*arrayInstance)[i] = static_cast<float>(i);
    }

    SPDLOG_INFO("Input data prepared (first 5 values: {}, {}, {}, {}, {})",
        arrayInstance->Get<float>(0),
        arrayInstance->Get<float>(1),
        arrayInstance->Get<float>(2),
        arrayInstance->Get<float>(3),
        arrayInstance->Get<float>(4));

    // Set array as material parameter
    SPDLOG_INFO("Setting array parameter in material...");
    Material* mat = materialManager.getMaterial(smartMaterial.handle());
    if (!mat) {
        SPDLOG_ERROR("Failed to get material pointer");
        return;
    }

    // Set the array as a BufferValue parameter
    ShaderLib::BufferValue bufferValue = arrayInstance;
    bool paramSet = mat->setParameter("values", bufferValue);

    if (!paramSet) {
        SPDLOG_ERROR("Failed to set array parameter in material");
        return;
    }

    SPDLOG_INFO("Array parameter set successfully");

    // Dispatch compute shader using the material
    SPDLOG_INFO("Dispatching compute shader...");
    SPDLOG_INFO("  Using automatic workgroup calculation for {} elements", DATA_SIZE);

    bool dispatchSuccess = computeDispatcher.dispatchForDataSize(
        smartMaterial,
        DATA_SIZE,  // dataSizeX - will be divided by local_size_x (256)
        1,          // dataSizeY
        1           // dataSizeZ
    );

    if (!dispatchSuccess) {
        SPDLOG_ERROR("Compute dispatch failed");
        return;
    }

    SPDLOG_INFO("Compute shader executed successfully");

    // Read back results from material parameter
    SPDLOG_INFO("Reading results from material...");

    Material::ParamValue resultParam;
    bool getSuccess = mat->getParameter("values", resultParam);

    if (!getSuccess) {
        SPDLOG_ERROR("Failed to get parameter from material");
        return;
    }

    // Extract the array instance from the parameter
    auto resultBufferValue = std::get_if<ShaderLib::BufferValue>(&resultParam);
    if (!resultBufferValue) {
        SPDLOG_ERROR("Parameter is not a BufferValue");
        return;
    }

    auto resultArray = std::get_if<std::shared_ptr<ShaderLib::ShaderArrayInstance>>(resultBufferValue);
    if (!resultArray || !(*resultArray)) {
        SPDLOG_ERROR("BufferValue does not contain a ShaderArrayInstance");
        return;
    }

    SPDLOG_INFO("Results array retrieved from material");

    // Extract float values from array
    std::vector<float> outputData;
    try {
        outputData = (*resultArray)->ToVectorOf<float>();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to extract float vector: {}", e.what());
        return;
    }

    SPDLOG_INFO("Results extracted successfully");

    // Verify results
    SPDLOG_INFO("Verifying results...");
    SPDLOG_INFO("Expected: each value should be doubled");
    SPDLOG_INFO("Output data (first 10 values):");

    bool allCorrect = true;
    for (uint32_t i = 0; i < std::min(10u, DATA_SIZE); ++i) {
        float expected = static_cast<float>(i) * 2.0f;
        bool correct = std::abs(outputData[i] - expected) < 0.001f;
        allCorrect &= correct;

        SPDLOG_INFO("  [{}] Input: {:.1f}, Output: {:.1f}, Expected: {:.1f} {}",
            i, static_cast<float>(i), outputData[i], expected,
            correct ? "✓" : "✗");
    }

    // Check all values
    for (uint32_t i = 10; i < DATA_SIZE; ++i) {
        float expected = static_cast<float>(i) * 2.0f;
        if (std::abs(outputData[i] - expected) >= 0.001f) {
            allCorrect = false;
            SPDLOG_ERROR("  [{}] Mismatch! Input: {:.1f}, Output: {:.1f}, Expected: {:.1f}",
                i, static_cast<float>(i), outputData[i], expected);
        }
    }

    if (allCorrect) {
        SPDLOG_INFO("=== TEST PASSED: All values correctly doubled ===");
    }
    else {
        SPDLOG_ERROR("=== TEST FAILED: Some values incorrect ===");
    }

    // Cleanup handled automatically by smart handles
    SPDLOG_INFO("=== Compute Shader Test Complete ===");
}