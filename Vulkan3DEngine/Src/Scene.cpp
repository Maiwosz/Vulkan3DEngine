#include "Scene.h"
#include <algorithm>
#include "TransformComponent.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"

Scene::Scene()
{

	m_registry = std::make_unique<Registry>();
	m_registry->getSystemManager().registerSystem<AssetCollectionSystem>();
    //m_registry->getSystemManager().registerSystem<RenderSystem>();
    //m_registry->getSystemManager().registerSystem<LightSystem>();
    //m_registry->getSystemManager().registerSystem<CameraSystem>();

    testEntity = m_registry->create();

    // Dodawanie komponentu transformacji
    m_registry->addComponent<TransformComponent>(testEntity);

    // Dodawanie komponentu meshu
    auto& mesh = m_registry->addComponent<MeshComponent>(testEntity);
    mesh.setMesh (AssetHandle(AssetLib::AssetType::Mesh, "Flora_C"));

    
    // Dodawanie komponentu materiału
    auto& material = m_registry->addComponent<MaterialComponent>(testEntity);
    material.setMaterial(AssetHandle(AssetLib::AssetType::Material, "Flora_C"));

    testCamera = m_registry->create();

    // Dodawanie komponentu transformacji
    m_registry->addComponent<TransformComponent>(testCamera);

    // Dodawanie komponentu camera
    m_registry->addComponent<CameraComponent>(testEntity);

    testDirectionalLight = m_registry->create();

	// Dodawanie komponentu DireactionalLight
    m_registry->addComponent<DirectionalLightComponent>(testDirectionalLight);

    testPointLight = m_registry->create();

	// Dodawanie komponentu pointLight
    m_registry->addComponent<PointLightComponent>(testPointLight);
}

Scene::~Scene()
{

}

void Scene::update()
{
    m_registry->getSystemManager().updateAll();
}
