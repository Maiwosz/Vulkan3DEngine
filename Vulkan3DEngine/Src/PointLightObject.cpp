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

PointLightObject::~PointLightObject()
{
    std::cout << "Rozmiar m_pointLights: " << p_scene->m_pointLights.size() << std::endl;
    auto it = std::find(p_scene->m_pointLights.begin(), p_scene->m_pointLights.end(), m_light);
    if (it != p_scene->m_pointLights.end())
    {
        p_scene->m_pointLights.erase(it);
    }
    std::cout << "Rozmiar m_pointLights po usuniêiu m_light: " << p_scene->m_pointLights.size() << std::endl;
}

void PointLightObject::update()
{
    SceneObject::update();
    m_light->position = m_position;
}

void PointLightObject::draw()
{
}
