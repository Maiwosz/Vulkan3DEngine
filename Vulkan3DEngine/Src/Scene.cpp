#include "Scene.h"
#include "Application.h"
#include "UniformBuffer.h"
#include "Descriptors.h"
#include "SceneObjectManager.h"
#include "Camera.h"
#include "InputSystem.h"
#include "RendererInits.h"
#include "AnimationBuilder.h"
#include "Animation.h"

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

    glm::vec3 centerPoint = glm::vec3(0.0f, 6.0f, 0.0f);
    float radius = 8.0f;
    float lightRotationSpeed = 45.0f;
    float duration = 360.0f / lightRotationSpeed;

    auto light1Sequence = AnimationBuilder()
        .orbit(m_pointLight1.get(), glm::vec3(0.0f, 6.0f, 0.0f), 8.0f, 45.0f, duration, glm::vec3(0.0f, 1.0f, 0.0f), 0)
        .build();

    auto light2Sequence = AnimationBuilder()
        .orbit(m_pointLight2.get(), glm::vec3(0.0f, 6.0f, 0.0f), 8.0f, 45.0f, duration, glm::vec3(0.0f, 1.0f, 0.0f), 120)
        .build();

    auto light3Sequence = AnimationBuilder()
        .orbit(m_pointLight3.get(), glm::vec3(0.0f, 6.0f, 0.0f), 8.0f, 45.0f, duration, glm::vec3(0.0f, 1.0f, 0.0f), 240)
        .build();

    // Assign animation sequences to lights
    m_pointLight1->setAnimationSequence(light1Sequence);
    m_pointLight2->setAnimationSequence(light2Sequence);
    m_pointLight3->setAnimationSequence(light3Sequence);



    // Tworzenie obiektu AnimationBuilder
    AnimationBuilder builder1;

    // Definiowanie pozycji startowej i koñcowej
    glm::vec3 startPosition1 = { 6.0f, 0.0f, -6.0f };
    glm::vec3 endPosition1 = { 6.0f, 0.0f, 6.0f };

    // Definiowanie rotacji startowej i koñcowej
    glm::vec3 startRotation1 = { 0.0f, 0.0f, 0.0f };
    glm::vec3 endRotation1 = { 0.0f, 180.0f, 0.0f }; // Obrót o 180 stopni

    // Definiowanie czasu trwania dla przesuniêcia i rotacji
    float moveDuration = 3.0f; // sekundy
    float rotateDuration = 2.0f; // sekundy

    // Dodawanie animacji do budowniczego
    builder1.move(m_statue1.get(), startPosition1, endPosition1, moveDuration);
    builder1.rotate(m_statue1.get(), startRotation1, endRotation1, rotateDuration);
    builder1.wait(moveDuration+ rotateDuration);
    builder1.move(m_statue1.get(), endPosition1, startPosition1, moveDuration);
    builder1.rotate(m_statue1.get(), endRotation1, startRotation1, rotateDuration);
    builder1.wait(moveDuration + rotateDuration);

    // Budowanie sekwencji animacji
    std::shared_ptr<AnimationSequence> sequence1 = builder1.build();
    sequence1->setLoop(true);

    m_statue1->setAnimationSequence(sequence1);

    // Tworzenie obiektu AnimationBuilder
    AnimationBuilder builder2;

    // Definiowanie pozycji startowej i koñcowej
    glm::vec3 startPosition2 = { -6.0f, 0.0f, 6.0f };
    glm::vec3 endPosition2 = { -6.0f, 0.0f, -6.0f };

    // Definiowanie rotacji startowej i koñcowej
    glm::vec3 startRotation2 = { 0.0f, 180.0f, 0.0f };
    glm::vec3 endRotation2 = { 0.0f, 0.0f, 0.0f }; // Obrót o 180 stopni

    // Dodawanie animacji do budowniczego
    builder2.wait(moveDuration + rotateDuration);
    builder2.move(m_statue2.get(), startPosition2, endPosition2, moveDuration);
    builder2.rotate(m_statue2.get(), startRotation2, endRotation2, rotateDuration);
    builder2.wait(moveDuration + rotateDuration);
    builder2.move(m_statue2.get(), endPosition2, startPosition2, moveDuration);
    builder2.rotate(m_statue2.get(), endRotation2, startRotation2, rotateDuration);

    // Budowanie sekwencji animacji
    std::shared_ptr<AnimationSequence> sequence = builder2.build();
    sequence->setLoop(true);

    m_statue2->setAnimationSequence(sequence);

    m_floor.reset();
    m_statue1.reset();
    m_statue2.reset();
    m_statue3.reset();
    m_statue4.reset();
    m_pointLight1.reset();
    m_pointLight2.reset();
    m_pointLight3.reset();
}

Scene::~Scene()
{
    m_globalDescriptorAllocator.destroyPool(GraphicsEngine::get()->getDevice()->get());
}

void Scene::update()
{
    ubo = GlobalUBO{};

    ubo.ambientCoefficient = 0.005f;
    
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
