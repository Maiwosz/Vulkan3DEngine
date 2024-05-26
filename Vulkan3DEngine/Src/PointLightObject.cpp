#include "PointLightObject.h"
#include "Scene.h"
#include <iostream>

PointLightObject::PointLightObject(Scene* scene) : SceneObject(scene, "PointLight")
{
    m_light = std::make_shared<PointLight>();
    m_position = glm::vec3(0.0f, 0.0f, 0.0f);
    m_light->position = m_position;
    m_light->color = glm::vec4(1.0f, 1.0f, 1.0f, 10.0f); // Default white color with intensity of 10.0f
    m_light->radius = 0.2f;
    p_scene->m_pointLights.push_back(m_light);
}

PointLightObject::PointLightObject(glm::vec3 color, float intensity, Scene* scene) : SceneObject(scene, "PointLight")
{
    m_light = std::make_shared<PointLight>();
    m_position = glm::vec3(0.0f, 0.0f, 0.0f);
    m_light->position = m_position;
    m_light->color = glm::vec4(color, intensity);
    m_light->radius = 0.1f;
    p_scene->m_pointLights.push_back(m_light);
}

PointLightObject::PointLightObject(float radius, glm::vec3 color, float intensity, Scene* scene) : SceneObject(scene, "PointLight")
{
    m_light = std::make_shared<PointLight>();
    m_position = glm::vec3(0.0f, 0.0f, 0.0f);
    m_light->position = m_position;
    m_light->color = glm::vec4(color, intensity);
    m_light->radius = radius;
    p_scene->m_pointLights.push_back(m_light);
}

PointLightObject::PointLightObject(glm::vec3 position, float radius, glm::vec3 color, float intensity, Scene* scene) : SceneObject(scene, "PointLight")
{
    m_light = std::make_shared<PointLight>();
    m_position = position;
    m_light->position = position;
    m_light->color = glm::vec4(color, intensity);
    m_light->radius = radius;
    p_scene->m_pointLights.push_back(m_light);
}

PointLightObject::PointLightObject(const nlohmann::json& j, Scene* scene) : SceneObject(j["SceneObjectData"], scene) {
    try {
        m_light = std::make_shared<PointLight>();
        from_json(j);
        m_light->position = m_position;
        p_scene->m_pointLights.push_back(m_light);
        fmt::print("PointLightObject initialized successfully\n");
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Error: Failed to initialize PointLightObject. Exception: {}\n", e.what());
        throw;
    }
}


PointLightObject::~PointLightObject()
{
    auto it = std::find(p_scene->m_pointLights.begin(), p_scene->m_pointLights.end(), m_light);
    if (it != p_scene->m_pointLights.end())
    {
        p_scene->m_pointLights.erase(it);
    }
}

void PointLightObject::update()
{
    SceneObject::update();
    m_light->position = m_position;
}

void PointLightObject::draw()
{
}

void PointLightObject::to_json(nlohmann::json& j) {
    nlohmann::json SceneObjectJson;
    SceneObject::to_json(SceneObjectJson); // Call base class method

    j = nlohmann::json{
        {"type", "pointLight"},
        {"SceneObjectData", SceneObjectJson},
        {"color", {m_light->color.x, m_light->color.y, m_light->color.z}},
        {"intensity", m_light->color.w},
        {"radius",  m_light->radius},
    };
}

void PointLightObject::from_json(const nlohmann::json& j) {
    try {
        m_light->color = glm::vec4(j["color"][0], j["color"][1], j["color"][2], j["intensity"]);
        m_light->radius = j["radius"];
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Error: Failed to deserialize PointLightObject. Exception: {}\n", e.what());
        throw; // Rzuæmy wyj¹tek dalej, aby ³atwiej by³o œledziæ b³¹d
    }
}


