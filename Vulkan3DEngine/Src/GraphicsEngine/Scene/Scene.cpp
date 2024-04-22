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

    m_statue1 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue1->setTranslation(5.0f, 0.0f, 0.0f);
    m_statue1->setRotationY(0.0f);

    m_statue2 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue2->setTranslation(-5.0f, 0.0f, 0.0f);
    m_statue2->setRotationY(90.0f);

    m_statue3 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue3->setTranslation(0.0f, 0.0f, 5.0f);
    m_statue3->setRotationY(180.0f);

    m_statue4 = GraphicsEngine::get()->getModelManager()->createModelInstance("Statue.JSON");
    m_statue4->setTranslation(0.0f, 0.0f, -5.0f);
    m_statue4->setRotationY(270.0f);

    //m_hygieia = GraphicsEngine::get()->getModelManager()->createModelInstance("Hygieia.JSON");
    //m_hygieia->setTranslation(0.0f, 0.0f, 0.0f);
    //m_hygieia->setRotationY(-90.0f);

    m_vikingRoom = GraphicsEngine::get()->getModelManager()->createModelInstance("viking_room.JSON");
    m_vikingRoom->setTranslation(0.0f, -10.0f, 0.0f);
}

Scene::~Scene()
{
}

void Scene::update()
{
    VkExtent2D swapChainExtent = GraphicsEngine::get()->getRenderer()->getSwapChain()->getSwapChainExtent();

    //Update uniform buffer
    GlobalUBO ubo{};//Eye position(left/right,forward/backward,height), What it looks at,  Where is up
    ubo.view = m_camera->getViewMatrix();
    ubo.proj = m_camera->getProjectionMatrix(swapChainExtent.width, swapChainExtent.height);
    memcpy(m_uniformBuffers[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->getMappedMemory(), &ubo, sizeof(ubo));
    //ubo.view = glm::lookAt(glm::vec3(0.0f, 4.5f, 3.0f), glm::vec3(0.0f, 0.0f, 0.85f), glm::vec3(0.0f, 0.0f, 0.1f));
    //ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
    //ubo.proj[1][1] *= -1;
    //memcpy(m_uniformBuffers[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->getMappedMemory(), &ubo, sizeof(ubo));

    // Define the rotation speed
    float rotationSpeed = 45.0f;
    m_statue1->setRotationY(m_statue1->getRotation().y + Application::s_deltaTime * rotationSpeed);
    m_statue2->setRotationY(m_statue1->getRotation().y + Application::s_deltaTime * rotationSpeed + 90);
    m_statue3->setRotationY(m_statue1->getRotation().y + Application::s_deltaTime * rotationSpeed + 180);
    m_statue4->setRotationY(m_statue1->getRotation().y + Application::s_deltaTime * rotationSpeed + 270);
    //m_hygieia->setRotationY(Application::s_deltaTime * rotationSpeed);

    //rect->update();
    m_statue1->update();
    m_statue2->update();
    m_statue3->update();
    m_statue4->update();
    m_vikingRoom->update();
    //m_castle->update();
    //m_hygieia->update();
}

void Scene::draw()
{
    m_globalDescriptorSets[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->bind();
    m_statue1->draw();
    m_statue2->draw();
    m_statue3->draw();
    m_statue4->draw();
    m_vikingRoom->draw();
    //m_castle->draw();
    //m_hygieia->draw();
}
