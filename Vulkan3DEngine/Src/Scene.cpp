#include "Scene.h"
#include "Application.h"
#include "UniformBuffer.h"
#include "GlobalDescriptorSet.h"
#include "SceneObjectManager.h"
#include "Camera.h"
#include "InputSystem.h"

Scene::Scene()
{
    int maxFramesInFlight = GraphicsEngine::get()->getRenderer()->s_maxFramesInFlight;

    m_uniformBuffers.resize(maxFramesInFlight);

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        m_uniformBuffers[i] = GraphicsEngine::get()->getRenderer()->createUniformBuffer(sizeof(GlobalUBO));
    }

    m_globalDescriptorSets.resize(maxFramesInFlight);

    for (int i = 0; i < maxFramesInFlight; i++) {
        m_globalDescriptorSets[i] = GraphicsEngine::get()->getRenderer()->createGlobalDescriptorSet(m_uniformBuffers[i]->get());
    }

    m_sceneObjectManager = std::make_shared<SceneObjectManager>(this);

    m_camera = m_sceneObjectManager->createCamera(glm::vec3(-8.0f, 8.0f, 8.0f), -30.0f, 45.0f); 

    m_light.direction = glm::vec3(0.0f, -1.0f, -1.0f);
    m_light.intensity = 0.0f;

    m_pointLight1.color = glm::vec3(1.0f, 1.0f, 1.0f);
    m_pointLight1.radius = 12.0f;
    m_pointLight1.intensity = 1.2f;

    //m_pointLight2.color = glm::vec3(0.0f, 0.3f, 0.0f);
    //m_pointLight2.radius = 5.0f;
    //m_pointLight2.intensity = 1.0f;
    //            
    //m_pointLight2Sphere = m_sceneObjectManager->createModel("Sphere.JSON");
    //m_pointLight2Sphere->setScale(0.2);
    //
    //m_pointLight3.color = glm::vec3(0.0f, 0.0f, 0.3f);
    //m_pointLight3.radius = 5.0f;
    //m_pointLight3.intensity = 1.0f;
    //            
    //m_pointLight3Sphere = m_sceneObjectManager->createModel("Sphere.JSON");
    //m_pointLight3Sphere->setScale(0.2);

    //m_floor = m_sceneObjectManager->createModel("Floor.JSON");
    //m_statue1 = m_sceneObjectManager->createModel("Statue.JSON");
    //m_statue2 = m_sceneObjectManager->createModel("Flora_C.JSON");
    //m_statue3 = m_sceneObjectManager->createModel("Hygieia_C.JSON");
    //m_statue4 = m_sceneObjectManager->createModel("Omphale_C.JSON");
    //m_pointLight1Sphere = m_sceneObjectManager->createModel("Sphere.JSON");

    std::vector<std::future<ModelPtr>> futures;
    
    // Dodaj zadania do puli w¹tków
    futures.push_back(ThreadPool::get()->enqueue([this]()->ModelPtr { return m_sceneObjectManager->createModel("Floor.JSON"); }));
    futures.push_back(ThreadPool::get()->enqueue([this]()->ModelPtr { return m_sceneObjectManager->createModel("Statue.JSON"); }));
    futures.push_back(ThreadPool::get()->enqueue([this]()->ModelPtr { return m_sceneObjectManager->createModel("Flora_C.JSON"); }));
    futures.push_back(ThreadPool::get()->enqueue([this]()->ModelPtr { return m_sceneObjectManager->createModel("Hygieia_C.JSON"); }));
    futures.push_back(ThreadPool::get()->enqueue([this]()->ModelPtr { return m_sceneObjectManager->createModel("Omphale_C.JSON"); }));
    futures.push_back(ThreadPool::get()->enqueue([this]()->ModelPtr { return m_sceneObjectManager->createModel("Sphere.JSON"); }));
    
    // Oczekuj na wyniki zadañ
    m_floor = futures[0].get();
    m_statue1 = futures[1].get();
    m_statue2 = futures[2].get();
    m_statue3 = futures[3].get();
    m_statue4 = futures[4].get();
    m_pointLight1Sphere = futures[5].get();
    
    m_pointLight1Sphere->setScale(0.2f);
    
    m_statue1->move(5.0f, 0.0f, 0.0f);
    m_statue1->rotate(0.0f, 90.0f, 0.0f);
    m_statue1->m_shininess = 0.3f;
    
    m_statue2->move(-5.0f, 0.0f, 0.0f);
    m_statue2->rotate(0.0f, -90.0f, 0.0f);
    m_statue2->m_shininess = 0.3f;
    
    m_statue3->move(0.0f, 0.0f, 5.0f);
    m_statue3->rotate(0.0f, 90.0f, 0.0f);
    m_statue3->m_shininess = 0.3f;
    
    m_statue4->move(0.0f, 0.0f, -5.0f);
    m_statue4->rotate(0.0f, 180.0f, 0.0f);
    m_statue4->m_shininess = 0.3f;
}

Scene::~Scene()
{
}

void Scene::update()
{
    ubo = GlobalUBO{};

    ubo.ambientCoefficient = 0.005f;

    // Define the rotation speed for the light
    float lightRotationSpeed = 45.0f;

    //Point light rotation
    glm::vec3 centerPoint = glm::vec3(0.0f, 6.0f, 0.0f);
    float radius = 8.0f;

    // Calculate the new light position
    m_lightAngle += Application::s_deltaTime * lightRotationSpeed;

    m_pointLight1.position.x = centerPoint.x + radius * cos(glm::radians(m_lightAngle));
    m_pointLight1.position.y = centerPoint.y;
    m_pointLight1.position.z = centerPoint.z + radius * sin(glm::radians(m_lightAngle));
    m_pointLight1Sphere->setPosition(m_pointLight1.position);

    //m_pointLight2.position.x = centerPoint.x + radius * cos(glm::radians(m_lightAngle + 120));
    //m_pointLight2.position.y = centerPoint.y;
    //m_pointLight2.position.z = centerPoint.z + radius * sin(glm::radians(m_lightAngle + 120));
    //m_pointLight2Sphere->setPosition(m_pointLight2.position);
    //
    //m_pointLight3.position.x = centerPoint.x + radius * cos(glm::radians(m_lightAngle + 240));
    //m_pointLight3.position.y = centerPoint.y;
    //m_pointLight3.position.z = centerPoint.z + radius * sin(glm::radians(m_lightAngle + 240));
    //m_pointLight3Sphere->setPosition(m_pointLight3.position);
    
    m_sceneObjectManager->updateObjects();

    //Update uniform buffer
    
    ubo.directionalLight = m_light;
    ubo.pointLights[0] = m_pointLight1;
    ubo.pointLights[1] = m_pointLight2;
    ubo.pointLights[2] = m_pointLight3;
    ubo.activePointLightCount = 1;
    
    memcpy(m_uniformBuffers[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->getMappedMemory(), &ubo, sizeof(ubo));
}

void Scene::draw()
{
    m_globalDescriptorSets[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->bind();
    m_sceneObjectManager->drawObjects();
}
