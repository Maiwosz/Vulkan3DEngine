#pragma once
#include "Prerequisites.h"

class SceneObject
{
public:
    SceneObject(Scene* scene) : m_position(0.0f), m_rotation(0.0f), m_scale(1.0f), p_scene(scene) {}
    SceneObject(glm::vec3 position, glm::vec3 rotation, float scale, Scene* scene) :
        m_position(position), m_rotation(rotation), m_scale(scale), p_scene(scene) {}

    // Move the object relative to its current position
    void move(float dx, float dy, float dz) {
        m_position += glm::vec3(dx, dy, dz);
    }

    // Rotate the object relative to its current rotation
    void rotate(float pitch, float yaw, float roll) {
        m_rotation += glm::vec3(pitch, yaw, roll);
    }

    void setPosition(float x, float y, float z) {
        m_position = glm::vec3(x, y, z);
    }

    void setPosition(glm::vec3 vec) {
        m_position = vec;
    }

    void setRotation(float pitch, float yaw, float roll) {
        m_rotation = glm::vec3(pitch, yaw, roll);
    }

    void setRotation(glm::vec3 vec) {
        m_rotation = vec;
    }

    void setScale(float scale) {
        m_scale = scale;
    }

    virtual void update() {
        positionMatrix = glm::translate(glm::mat4(1.0f), m_position);
        rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale));
        //transformMatrix = scaleMatrix * translationMatrix * rotationMatrix;
    }

    virtual void draw() = 0;

    // Getter functions
    glm::vec3 getPosition() const { return m_position; }
    glm::vec3 getRotation() const { return m_rotation; }
    float getScale() const { return m_scale; }

    bool isActive = true;
protected:
    Scene* p_scene;
    glm::vec3 m_position = glm::vec3(0.0f);;
    glm::vec3 m_rotation = glm::vec3(0.0f);;
    float m_scale;

    glm::mat4 transformMatrix = glm::mat4(1.0f);
    glm::mat4 positionMatrix = glm::mat4(1.0f);
    glm::mat4 rotationMatrix = glm::mat4(1.0f);
    glm::mat4 scaleMatrix = glm::mat4(1.0f);
};

