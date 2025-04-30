#pragma once
#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
        model = glm::rotate(model, glm::radians(m_rotation.z), { 0,0,1 }) * // Roll
            glm::rotate(model, glm::radians(m_rotation.x), { 1,0,0 }) * // Pitch
            glm::rotate(model, glm::radians(m_rotation.y), { 0,1,0 });  // Yaw
        return glm::scale(model, m_scale);
    }

private:
    glm::vec3 m_position{ 0.0f };
    glm::vec3 m_rotation{ 0.0f }; // Degrees
    glm::vec3 m_scale{ 1.0f };
};