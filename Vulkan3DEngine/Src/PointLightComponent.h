#pragma once
#include "Component.h"
#include <glm/glm.hpp>

struct PointLightComponent : public Component {
public:
    void setColor(const glm::vec4& col) {
        m_color = col;
        incrementVersion();
    }

    void setRadius(float r) {
        m_radius = r;
        incrementVersion();
    }

    const glm::vec4& getColor() const { return m_color; }
    float getRadius() const { return m_radius; }

private:
    glm::vec4 m_color{ 1.0f, 1.0f, 1.0f, 1.0f };
    float m_radius = 10.0f;
};