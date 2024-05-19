#include "Scene.h"
#include "Application.h"
#include "UniformBuffer.h"
#include "Descriptors.h"
#include "SceneObjectManager.h"
#include "Camera.h"
#include "InputSystem.h"
#include "RendererInits.h"

#include <algorithm>

Scene::Scene()
{
    int maxFramesInFlight = Renderer::s_maxFramesInFlight;

    m_uniformBuffers.resize(maxFramesInFlight);

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        m_uniformBuffers[i] = GraphicsEngine::get()->getRenderer()->createUniformBuffer(sizeof(GlobalUBO));
    }

    m_globalDescriptorSets.resize(maxFramesInFlight);

    //create a descriptor pool that will hold <maxFramesInFlight> sets with 1 uniform buffer each
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
    };

    m_globalDescriptorAllocator.initPool(GraphicsEngine::get()->getDevice()->get(), maxFramesInFlight, sizes);

    DescriptorWriter writer;

    for (int i = 0; i < maxFramesInFlight; i++) {
        m_globalDescriptorSets[i] = m_globalDescriptorAllocator.allocate(GraphicsEngine::get()->getDevice()->get(),
            GraphicsEngine::get()->getRenderer()->m_globalDescriptorSetLayout);
        
        
        writer.writeBuffer(0, m_uniformBuffers[i]->get(), sizeof(GlobalUBO),0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer.updateSet(GraphicsEngine::get()->getDevice()->get(), m_globalDescriptorSets[i]);
        writer.clear();
    }

    m_sceneObjectManager = std::make_shared<SceneObjectManager>(this);

    m_camera = m_sceneObjectManager->createCamera(glm::vec3(-8.0f, 8.0f, 8.0f), -30.0f, 45.0f); 

    m_light.direction = glm::vec3(0.0f, -1.0f, -1.0f);
    m_light.color.w = 0.0f;

    m_pointLight1 = m_sceneObjectManager->createPointLight(0.2f, glm::vec3(1.0f, 0.0f, 0.0f), 10.0f);
    m_pointLight2 = m_sceneObjectManager->createPointLight(0.2f, glm::vec3(0.0f, 1.0f, 0.0f), 10.0f);
    m_pointLight3 = m_sceneObjectManager->createPointLight(0.2f, glm::vec3(0.0f, 0.0f, 1.0f), 10.0f);

    std::vector<std::future<ModelPtr>> futures;
    
    // Dodaj zadania do puli w¹tków
    futures.push_back(ThreadPool::get()->enqueue([this]()->ModelPtr { return m_sceneObjectManager->createModel("Floor.JSON"); }));
    futures.push_back(ThreadPool::get()->enqueue([this]()->ModelPtr { return m_sceneObjectManager->createModel("Statue.JSON"); }));
    futures.push_back(ThreadPool::get()->enqueue([this]()->ModelPtr { return m_sceneObjectManager->createModel("Flora_C.JSON"); }));
    futures.push_back(ThreadPool::get()->enqueue([this]()->ModelPtr { return m_sceneObjectManager->createModel("Hygieia_C.JSON"); }));
    futures.push_back(ThreadPool::get()->enqueue([this]()->ModelPtr { return m_sceneObjectManager->createModel("Omphale_C.JSON"); }));
    
    // Oczekuj na wyniki zadañ
    m_floor = futures[0].get();
    m_statue1 = futures[1].get();
    m_statue2 = futures[2].get();
    m_statue3 = futures[3].get();
    m_statue4 = futures[4].get();

    //m_floor->m_shininess = 1.0f;
    
    m_statue1->move(5.0f, 0.0f, 0.0f);
    m_statue1->rotate(0.0f, 90.0f, 0.0f);
    //m_statue1->m_shininess = 0.3f;
    
    m_statue2->move(-5.0f, 0.0f, 0.0f);
    m_statue2->rotate(0.0f, -90.0f, 0.0f);
    //m_statue2->m_shininess = 0.3f;
    
    m_statue3->move(0.0f, 0.0f, 5.0f);
    m_statue3->rotate(0.0f, 90.0f, 0.0f);
    //m_statue3->m_shininess = 0.3f;
    
    m_statue4->move(0.0f, 0.0f, -5.0f);
    m_statue4->rotate(0.0f, 180.0f, 0.0f);
    //m_statue4->m_shininess = 0.3f;
}

Scene::~Scene()
{
    m_globalDescriptorAllocator.destroyPool(GraphicsEngine::get()->getDevice()->get());
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

    float x = centerPoint.x + radius * cos(glm::radians(m_lightAngle));
    float y = centerPoint.y;
    float z = centerPoint.z + radius * sin(glm::radians(m_lightAngle));

    m_pointLight1->setPosition(x, y, z);

    x = centerPoint.x + radius * cos(glm::radians(m_lightAngle + 120));
    y = centerPoint.y;
    z = centerPoint.z + radius * sin(glm::radians(m_lightAngle + 120));

    m_pointLight2->setPosition(x, y, z);

    x = centerPoint.x + radius * cos(glm::radians(m_lightAngle + 240));
    y = centerPoint.y;
    z = centerPoint.z + radius * sin(glm::radians(m_lightAngle + 240));

    m_pointLight3->setPosition(x, y, z);
    
    m_sceneObjectManager->updateObjects();

    //Update uniform buffer

    auto distanceToCamera = [&](PointLightPtr light1, PointLightPtr light2) {
        float distance1 = glm::distance(light1->position, m_camera->getPosition());
        float distance2 = glm::distance(light2->position, m_camera->getPosition());
        return distance1 > distance2;
        };

    std::sort(m_pointLights.begin(), m_pointLights.end(), distanceToCamera);

    
    ubo.directionalLight = m_light;
    for (int i = 0; i < m_pointLights.size(); i++) {
        ubo.pointLights[i] = *m_pointLights[i];
        ubo.activePointLightCount = m_pointLights.size();
    }
    
    memcpy(m_uniformBuffers[GraphicsEngine::get()->getRenderer()->getCurrentFrame()]->getMappedMemory(), &ubo, sizeof(ubo));
}

void Scene::draw()
{
    GraphicsEngine::get()->getRenderer()->bindDescriptorSet(m_globalDescriptorSets[GraphicsEngine::get()->getRenderer()->getCurrentFrame()], 0);
    m_sceneObjectManager->drawObjects();
}
