#pragma once
#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
struct TransformComponent : public Component {
public:
    // Setters
    void setPosition(const glm::vec3& position) {
        m_position = position;
        incrementVersion();
    }
    void setRotation(const glm::vec3& rotation) {
        m_rotation = rotation;
        incrementVersion();
    }
    void setScale(const glm::vec3& scale) {
        m_scale = scale;
        incrementVersion();
    }
    // Getters
    const glm::vec3& getPosition() const { return m_position; }
    const glm::vec3& getRotation() const { return m_rotation; }
    const glm::vec3& getScale() const { return m_scale; }
    // Helper functions
    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), m_position);

        // Create a quaternion from Euler angles (in YXZ order)
        glm::quat quaternion = glm::quat(glm::vec3(
            glm::radians(m_rotation.x), // pitch (around X)
            glm::radians(m_rotation.y), // yaw (around Y)
            glm::radians(m_rotation.z)  // roll (around Z)
        ));

        // Apply rotation using the quaternion (avoids gimbal lock issues)
        model = model * glm::toMat4(quaternion);

        // Apply scaling
        model = glm::scale(model, m_scale);

        return model;
    }

    glm::mat4 getViewMatrix() const {
        // Create rotation quaternion using Euler angles in YXZ order
        // (yaw around Y, then pitch around X, then roll around Z)
        glm::quat quat = glm::quat(glm::vec3(
            glm::radians(m_rotation.x), // pitch (around X)
            glm::radians(m_rotation.y), // yaw (around Y)
            glm::radians(m_rotation.z)  // roll (around Z)
        ));

        // Get the camera's forward direction (-Z in view space)
        glm::vec3 forward = glm::normalize(
            quat * glm::vec3(0.0f, 0.0f, -1.0f)
        );

        // Get the camera's up direction (Y in view space)
        glm::vec3 up = glm::normalize(
            quat * glm::vec3(0.0f, 1.0f, 0.0f)
        );

        // Calculate the target point
        glm::vec3 target = m_position + forward;

        // Create the view matrix
        return glm::lookAt(m_position, target, up);
    }
private:
    glm::vec3 m_position{ 0.0f };
    glm::vec3 m_rotation{ 0.0f }; // Degrees
    glm::vec3 m_scale{ 1.0f };
};