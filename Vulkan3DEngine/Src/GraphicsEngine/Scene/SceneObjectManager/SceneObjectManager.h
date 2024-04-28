#pragma once
//#include <vector>
//#include <memory>
#include "../../../Prerequisites.h"
#include "../../../Application/Application.h"
#include <string>
#include "SceneObject.h"
#include "Model/Model.h"
#include "Camera/Camera.h"

class SceneObjectManager
{
public:
    SceneObjectManager(Scene* scene): p_scene(scene) {}
    ~SceneObjectManager() {}

    std::shared_ptr<Model> createModel(const std::string& name) {
        ModelPtr model = std::make_shared<Model>(GraphicsEngine::get()->getModelDataManager()->loadModelData(name), p_scene);
        m_objects.push_back(model);
        return model;
    }

    std::shared_ptr<Camera> createCamera(glm::vec3 startPosition, float startPitch, float startYaw) {
        std::shared_ptr<Camera> camera = std::make_shared<Camera>(startPosition, startPitch, startYaw, p_scene);
        m_objects.push_back(camera);
        return camera;
    }

    void updateObjects() {
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

    void drawObjects() {
        for (auto& object : m_objects) {
            object->draw();
        }
    }

    void removeObject(std::shared_ptr<SceneObject> object) {
        object->isActive = false;
    }

    std::vector<std::shared_ptr<SceneObject>> getObjects() const {
        return m_objects;
    }

private:
    Scene* p_scene;
    std::vector<std::shared_ptr<SceneObject>> m_objects;
};