#include "Scene.h"
#include "../../Application/Application.h"
#include "../Renderer/Buffers/UniformBuffer/UniformBuffer.h"
#include "../Renderer/DescriptorSets/DescriptorSet/GlobalDescriptorSet/GlobalDescriptorSet.h"
#include "SceneObjectManager/SceneObjectManager.h"
#include "SceneObjectManager/Camera/Camera.h"
#include "../../InputSystem/InputSystem.h"

Scene::Scene()
{
    m_uniformBuffers.resize(GraphicsEngine::get()->getRenderer()->MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < GraphicsEngine::get()->getRenderer()->MAX_FRAMES_IN_FLIGHT; i++) {
        m_uniformBuffers[i] = GraphicsEngine::get()->getRenderer()->createUniformBuffer(sizeof(GlobalUBO));
    }

    m_globalDescriptorSets.resize(GraphicsEngine::get()->getRenderer()->MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < GraphicsEngine::get()->getRenderer()->MAX_FRAMES_IN_FLIGHT; i++) {
        m_globalDescriptorSets[i] = GraphicsEngine::get()->getRenderer()->createGlobalDescriptorSet(m_uniformBuffers[i]->get());
    }

    m_sceneObjectManager = std::make_shared<SceneObjectManager>(this);

    m_camera = m_sceneObjectManager->createCamera(glm::vec3(-8.0f, 8.0f, 8.0f), -30.0f, 45.0f); 

    m_light.direction = glm::vec3(0.0f, 1.0f, 1.0f);
    m_light.intensity = 0.0f;

    m_pointLight1.color = glm::vec3(1.0f, 1.0f, 1.0f);
    m_pointLight1.radius = 5.0f;
    m_pointLight1.intensity = 1.0f;

    m_pointLight1Sphere = m_sceneObjectManager->createModel("Sphere.JSON");
    m_pointLight1Sphere->setScale(0.2);

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

    m_floor = m_sceneObjectManager->createModel("Floor.JSON");
    m_floor->m_shininess = 1.0f;

    m_statue1 = m_sceneObjectManager->createModel("Statue.JSON");
    m_statue1->move(5.0f, 0.0f, 0.0f);
    m_statue1->rotate(0.0f, 90.0f, 0.0f);
    m_statue1->m_shininess = 0.3f;

    m_statue2 = m_sceneObjectManager->createModel("Statue.JSON");
    m_statue2->move(-5.0f, 0.0f, 0.0f);
    m_statue2->rotate(0.0f, 270.0f, 0.0f);
    m_statue2->m_shininess = 0.3f;

    m_statue3 = m_sceneObjectManager->createModel("Statue.JSON");
    m_statue3->move(0.0f, 0.0f, 5.0f);
    m_statue3->rotate(0.0f, 0.0f, 0.0f);
    m_statue3->m_shininess = 0.3f;

    m_statue4 = m_sceneObjectManager->createModel("Statue.JSON");
    m_statue4->move(0.0f, 0.0f, -5.0f);
    m_statue4->rotate(0.0f, 180.0f, 0.0f);
    m_statue4->m_shininess = 0.3f;

    //m_hygieia = GraphicsEngine::get()->getModelManager()->createModelInstance("Hygieia.JSON");
    //m_hygieia->setTranslation(0.0f, 0.0f, 0.0f);
    //m_hygieia->setRotationY(-90.0f);

    //m_vikingRoom = GraphicsEngine::get()->getModelManager()->createModelInstance("viking_room.JSON");
    //m_vikingRoom->setTranslation(0.0f, -10.0f, 0.0f);
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
