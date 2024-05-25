#pragma once
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

    std::shared_ptr<Model> createModel(const std::string& modelName);
    std::shared_ptr<Model> createModel(const std::string name, const std::string meshName, const std::string textureName,
        const float scale, const float shininess, const float kd, const float ks, const float initialScale,
        const glm::vec3 initialPosition, const glm::vec3 initialRotation);

    std::shared_ptr<PointLightObject> createPointLight();
    std::shared_ptr<PointLightObject> createPointLight(glm::vec3 color, float intensity);
    std::shared_ptr<PointLightObject> createPointLight(float radius, glm::vec3 color, float intensity);
    std::shared_ptr<PointLightObject> createPointLight(glm::vec3 position, float radius, glm::vec3 color, float intensity);

    std::shared_ptr<Camera> createCamera(glm::vec3 startPosition, float startPitch, float startYaw);

    void updateObjects();

    void drawObjects();

    void removeObject(std::shared_ptr<SceneObject> object);
    void addObject(std::shared_ptr<SceneObject> object);

    void removeAllObjects() {
        m_objects.clear();
    }

    std::vector<std::shared_ptr<SceneObject>> getObjects() const {
        return m_objects;
    }

    void drawInterface();

    void to_json(nlohmann::json& j);
    void from_json(const nlohmann::json& j);
private:
    SceneObject* m_selectedObject = nullptr;
    Scene* p_scene;
    std::vector<std::shared_ptr<SceneObject>> m_objects;
};