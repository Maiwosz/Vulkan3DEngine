#include "Scene.h"
#include "../../Application/Application.h"
#include "../Renderer/Buffers/UniformBuffer/UniformBuffer.h"
#include "../Renderer/DescriptorSets/DescriptorSet/GlobalDescriptorSet/GlobalDescriptorSet.h"
#include "../Camera/Camera.h"
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

    m_camera = std::make_shared<Camera>(
        //???-???-height
        glm::vec3(0.0f, 0.0f, 2.0f), // position
        0.0f, // yaw 
        90.0f // pitch
    );  
     
    m_light.direction = glm::vec3(0.0f, 1.0f, 1.0f);
    m_pointLight1.intensity = 0.0f;

    m_pointLight1.color = glm::vec3(1.0f, 0.0f, 0.0f);
    m_pointLight1.intensity = 1.0f;

    m_pointLight2.color = glm::vec3(0.0f, 0.0f, 1.0f);
    m_pointLight1.intensity = 1.0f;

    m_pointLight3.color = glm::vec3(0.0f, 1.0f, 0.0f);
    m_pointLight3.intensity = 1.0f;

    m_floor = GraphicsEngine::get()->getModelManager()->createModelInstance("Floor.JSON");
    m_floor->setTranslation(0.0f, 0.0f, 0.0f);
    m_floor->m_shininess = 1.0f;
    //m_floor->setScale(1.0f);

    m_statue1 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue1->setTranslation(5.0f, 0.0f, 0.0f);
    m_statue1->setRotationY(90.0f);
    m_statue1->m_shininess = 0.3f;

    m_statue2 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue2->setTranslation(-5.0f, 0.0f, 0.0f);
    m_statue2->setRotationY(270.0f);
    m_statue2->m_shininess = 0.3f;

    m_statue3 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue3->setTranslation(0.0f, 0.0f, 5.0f);
    m_statue3->setRotationY(0.0f);
    m_statue3->m_shininess = 0.3f;

    m_statue4 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue4->setTranslation(0.0f, 0.0f, -5.0f);
    m_statue4->setRotationY(180.0f);
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
    VkExtent2D swapChainExtent = GraphicsEngine::get()->getRenderer()->getSwapChain()->getSwapChainExtent();

    //Directional light rotation
    // Define the rotation speed for the light
    float lightRotationSpeed = 45.0f;

    // Calculate the new light direction
    //float lightAngle = Application::s_deltaTime * lightRotationSpeed;
    //glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(lightAngle), glm::vec3(0.0f, 0.0f, 1.0f));
    //glm::vec4 newDirection = rotationMatrix * glm::vec4(m_light.direction, 0.0f);

    // Update the light direction
    //m_light.direction = glm::normalize(glm::vec3(newDirection));

    //Point ight rotation
    glm::vec3 centerPoint = glm::vec3(0.0f, 0.0f, 5.0f);
    float radius = 7.0f;

    // Calculate the new light position
    m_lightAngle += Application::s_deltaTime * lightRotationSpeed;
    m_pointLight1.position.x = centerPoint.x + radius * cos(glm::radians(m_lightAngle));
    m_pointLight1.position.y = centerPoint.y + radius * sin(glm::radians(m_lightAngle));

    m_pointLight2.position.x = centerPoint.x + radius * cos(glm::radians(m_lightAngle+180));
    m_pointLight2.position.y = centerPoint.y + radius * sin(glm::radians(m_lightAngle+180));

    m_pointLight3.position.x = centerPoint.x + radius * cos(glm::radians(m_lightAngle+240));
    m_pointLight3.position.y = centerPoint.y + radius * sin(glm::radians(m_lightAngle+240));

    // Define the rotation speed
    float rotationSpeed = 45.0f;
    //m_statue1->setRotationY(m_statue1->getRotation().y + Application::s_deltaTime * rotationSpeed);
    //m_statue2->setRotationY(m_statue1->getRotation().y + Application::s_deltaTime * rotationSpeed + 90);
    //m_statue3->setRotationY(m_statue1->getRotation().y + Application::s_deltaTime * rotationSpeed + 180);
    //m_statue4->setRotationY(m_statue1->getRotation().y + Application::s_deltaTime * rotationSpeed + 270);
    //m_hygieia->setRotationY(Application::s_deltaTime * rotationSpeed);

    //rect->update();
    m_floor->update();
    m_statue1->update();
    m_statue2->update();
    m_statue3->update();
    m_statue4->update();
    //m_vikingRoom->update();
    //m_castle->update();
    //m_hygieia->update();

    //Update uniform buffer
    GlobalUBO ubo{};
    ubo.view = m_camera->getViewMatrix();
    ubo.proj = m_camera->getProjectionMatrix(swapChainExtent.width, swapChainExtent.height);
    ubo.directionalLight = m_light;
    ubo.pointLights[0] = m_pointLight1;
    ubo.pointLights[1] = m_pointLight2;
    ubo.pointLights[2] = m_pointLight3;
    ubo.activePointLightCount = 2;
    ubo.cameraPosition = m_camera->m_position;
    memcpy(m_uniformBuffers[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->getMappedMemory(), &ubo, sizeof(ubo));
}

void Scene::draw()
{
    m_globalDescriptorSets[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->bind();
    m_floor->draw();
    m_statue1->draw();
    m_statue2->draw();
    m_statue3->draw();
    m_statue4->draw();
    //m_vikingRoom->draw();
    //m_castle->draw();
    //m_hygieia->draw();
}
