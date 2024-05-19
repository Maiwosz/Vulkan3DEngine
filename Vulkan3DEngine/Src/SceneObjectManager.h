#pragma once
//#include <vector>
//#include <memory>
#include "Prerequisites.h"
#include "Application.h"
#include <string>
#include "SceneObject.h"
#include "Model.h"
#include "Camera.h"
#include "PointLightObject.h"

class SceneObjectManager
{
public:
    SceneObjectManager(Scene* scene);
    ~SceneObjectManager() {}

    std::shared_ptr<Model> createModel(const std::string& name);

    std::shared_ptr<PointLightObject> createPointLight();
    std::shared_ptr<PointLightObject> createPointLight(glm::vec3 color, float intensity);
    std::shared_ptr<PointLightObject> createPointLight(float radius, glm::vec3 color, float intensity);
    std::shared_ptr<PointLightObject> createPointLight(glm::vec3 position, float radius, glm::vec3 color, float intensity);

    std::shared_ptr<Camera> createCamera(glm::vec3 startPosition, float startPitch, float startYaw);

    void updateObjects();

    void drawObjects();

    void removeObject(std::shared_ptr<SceneObject> object);

    std::vector<std::shared_ptr<SceneObject>> getObjects() const {
        return m_objects;
    }

    void drawInterface();

private:
    Scene* p_scene;
    std::vector<std::shared_ptr<SceneObject>> m_objects;
};