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

Scene::Scene()
{
    m_registry = std::make_unique<Registry>();
    auto& systems = m_registry->getSystemManager();
    systems.registerSystem<ScriptSystem>();
    systems.registerSystem<AssetCollectionSystem>();
    systems.registerSystem<MeshRenderSystem>();
    systems.registerSystem<LightSystem>();
    systems.registerSystem<CameraSystem>();

    systems.getSystem<ScriptSystem>().initialize();

    // Creating object with mesh
    testEntity = m_registry->create();
    {
        // Konfiguracja zasobów
        std::string meshName = "Flora_C";
        std::string materialName = "Flora_c";

        // Transform with rotation
        auto& transform = m_registry->addComponent<TransformComponent>(testEntity);
        transform.setPosition(glm::vec3(0.0f, 2.0f, 0.0f));

        transform.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));

        transform.setScale(glm::vec3(0.4f));

        // Mesh and material
        auto& mesh = m_registry->addComponent<MeshComponent>(testEntity);
        mesh.setMesh(AssetHandle(AssetLib::AssetType::Mesh, meshName));

        auto& material = m_registry->addComponent<MaterialComponent>(testEntity);
        material.setMaterial(AssetHandle(AssetLib::AssetType::Material, materialName));
    }

    testEntity2 = m_registry->create();
    {
        // Konfiguracja zasobów
        std::string meshName = "Hygieia_C";
        std::string materialName = "Hygieia_C";

        // Transform with rotation
        auto& transform = m_registry->addComponent<TransformComponent>(testEntity2);
        transform.setPosition(glm::vec3(5.0f, -0.2f, 0.0f));

        transform.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));

        transform.setScale(glm::vec3(1.0f));

        // Mesh and material
        auto& mesh = m_registry->addComponent<MeshComponent>(testEntity2);
        mesh.setMesh(AssetHandle(AssetLib::AssetType::Mesh, meshName));

        auto& material = m_registry->addComponent<MaterialComponent>(testEntity2);
        material.setMaterial(AssetHandle(AssetLib::AssetType::Material, materialName));
    }

    testEntity3 = m_registry->create();
    {
        // Konfiguracja zasobów
        std::string meshName = "Omphale_C";
        std::string materialName = "Omphale_C";

        // Transform with rotation
        auto& transform = m_registry->addComponent<TransformComponent>(testEntity3);
        transform.setPosition(glm::vec3(-5.0f, 0.0f, 0.0f));

        // Apply rotation - 30 degrees around Y axis
        float rotationAngle = 30.0f;
        glm::vec3 rotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
        transform.setRotation(glm::vec3(0.0f, rotationAngle, 0.0f));

        transform.setScale(glm::vec3(0.3f));

        // Mesh and material
        auto& mesh = m_registry->addComponent<MeshComponent>(testEntity3);
        mesh.setMesh(AssetHandle(AssetLib::AssetType::Mesh, meshName));

        auto& material = m_registry->addComponent<MaterialComponent>(testEntity3);
        material.setMaterial(AssetHandle(AssetLib::AssetType::Material, materialName));
    }

    // floor
    testFloor = m_registry->create();
    {
        // Konfiguracja zasobów
        std::string meshName = "quad";
        std::string materialName = "floor";

        // Transform with rotation
        auto& transform = m_registry->addComponent<TransformComponent>(testFloor);
        transform.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));

        transform.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));

        transform.setScale(glm::vec3(1.0f));

        // Mesh and material
        auto& mesh = m_registry->addComponent<MeshComponent>(testFloor);
        mesh.setMesh(AssetHandle(AssetLib::AssetType::Mesh, meshName));

        auto& material = m_registry->addComponent<MaterialComponent>(testFloor);
        material.setMaterial(AssetHandle(AssetLib::AssetType::Material, materialName));
    }

    // Camera with script
    testCamera = m_registry->create();
    {
        auto& transform = m_registry->addComponent<TransformComponent>(testCamera);

        // Initial camera position (will be overridden by script)
        transform.setPosition(glm::vec3(0.0f, 2.0f, 15.0f));
        transform.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));

        // Add camera component
        auto& camera = m_registry->addComponent<CameraComponent>(testCamera);
        float fieldOfView = 45.0f;
        float aspectRatio = 1920.0f / 1080.0f;
        float nearPlane = 0.1f;
        float farPlane = 100.0f;
        camera.setVerticalFOV(fieldOfView);
        camera.setAspectRatio(aspectRatio);
        camera.setClippingPlanes(nearPlane, farPlane);

        // Add light orbiter script
        auto& script = m_registry->addComponent<ScriptComponent>(testCamera);
        script.setScriptPath("CameraController");
    }

    // Directional light
    testDirectionalLight = m_registry->create();
    {
        auto& transform = m_registry->addComponent<TransformComponent>(testDirectionalLight);
        auto& light = m_registry->addComponent<LightComponent>(testDirectionalLight, LightComponent::Type::Directional);

        glm::vec3 directionalLightDir = glm::vec3(1.0f, -1.0f, 1.0f);
        glm::vec4 directionalLightColor = glm::vec4(1.0f, 0.8f, 0.8f, 0.005f);

        light.setDirection(directionalLightDir);
        light.setColor(directionalLightColor);
    }

    // Point light with orbit script
    testPointLight = m_registry->create();
    {
        auto& transform = m_registry->addComponent<TransformComponent>(testPointLight);

        // Initial position (will be overridden by script)
        transform.setPosition(glm::vec3(7.0f, 3.0f, 0.0f));

        // Add light component
        auto& light = m_registry->addComponent<LightComponent>(testPointLight, LightComponent::Type::Point);
        light.setRadius(12.0f);
        light.setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        // Add light orbiter script
        auto& script = m_registry->addComponent<ScriptComponent>(testPointLight);
        script.setScriptPath("LightOrbiter");
    }
}

Scene::~Scene()
{

}

void Scene::update()
{
    m_registry->getSystemManager().updateAll();
}