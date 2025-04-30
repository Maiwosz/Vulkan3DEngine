#pragma once
#include "Component.h"
#include <glm/glm.hpp>

struct DirectionalLightComponent : public Component {
public:
    void setDirection(const glm::vec3& dir) {
        m_direction = glm::normalize(dir);
        incrementVersion();
    }

    void setColor(const glm::vec4& col) {
        m_color = col;
        incrementVersion();
    }

    const glm::vec3& getDirection() const { return m_direction; }
    const glm::vec4& getColor() const { return m_color; }

private:
    glm::vec3 m_direction{ 0.0f, -1.0f, 0.0f };
    glm::vec4 m_color{ 1.0f, 1.0f, 1.0f, 1.0f };
};