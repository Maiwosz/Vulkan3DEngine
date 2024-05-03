#pragma once
//#include <vector>
//#include <memory>
#include "Prerequisites.h"
#include "Application.h"
#include <string>
#include "SceneObject.h"
#include "Model.h"
#include "Camera.h"

class SceneObjectManager
{
public:
    SceneObjectManager(Scene* scene);
    ~SceneObjectManager() {}

    std::shared_ptr<Model> createModel(const std::string& name);

    std::shared_ptr<Camera> createCamera(glm::vec3 startPosition, float startPitch, float startYaw);

    void updateObjects();

    void drawObjects();

    void removeObject(std::shared_ptr<SceneObject> object);

    void loadResources();

    std::vector<std::shared_ptr<SceneObject>> getObjects() const {
        return m_objects;
    }

private:
    Scene* p_scene;
    std::vector<std::shared_ptr<SceneObject>> m_objects;
};