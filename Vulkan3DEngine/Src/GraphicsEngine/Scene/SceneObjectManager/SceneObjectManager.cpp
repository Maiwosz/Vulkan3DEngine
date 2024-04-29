#include "SceneObjectManager.h"
#include <future>

SceneObjectManager::SceneObjectManager(Scene* scene)
    : p_scene(scene) 
{

}

std::shared_ptr<Model> SceneObjectManager::createModel(const std::string& name)
{
    ModelPtr model = std::make_shared<Model>(GraphicsEngine::get()->getModelDataManager()->loadModelData(name), p_scene);
    m_objects.push_back(model);
    return model;
}

std::shared_ptr<Camera> SceneObjectManager::createCamera(glm::vec3 startPosition, float startPitch, float startYaw)
{
    std::shared_ptr<Camera> camera = std::make_shared<Camera>(startPosition, startPitch, startYaw, p_scene);
    m_objects.push_back(camera);
    return camera;
}

void SceneObjectManager::updateObjects()
{
    auto it = m_objects.begin();
    while (it != m_objects.end()) {
        if ((*it)->isActive) {
            (*it)->update();
            ++it;
        }
        else {
            it = m_objects.erase(it);
        }
    }
}

void SceneObjectManager::drawObjects()
{
    for (auto& object : m_objects) {
        object->draw();
    }
}

void SceneObjectManager::removeObject(std::shared_ptr<SceneObject> object)
{
    object->isActive = false;
}
