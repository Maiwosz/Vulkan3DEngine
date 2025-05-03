#pragma once
#include "Component.h"
#include <glm/glm.hpp>
#include <cassert>

struct LightComponent : public Component {
    enum class Type { Directional, Point };

    Type type;
    glm::vec4 color;

    // Dane specyficzne dla typu światła
    glm::vec3 direction; // Używane tylko dla Type::Directional
    float radius;        // Używane tylko dla Type::Point

    explicit LightComponent(Type lightType) : type(lightType) {
        color = glm::vec4(1.0f);
        if (type == Type::Directional) {
            direction = glm::vec3(0.0f, -1.0f, 0.0f);
            radius = 0.0f; // Nieużywane
        }
        else {
            direction = glm::vec3(0.0f); // Nieużywane
            radius = 10.0f;
        }
    }

    // Metody dla światła kierunkowego
    void setDirection(const glm::vec3& dir) {
        assert(type == Type::Directional && "Not a directional light");
        direction = glm::normalize(dir);
        incrementVersion();
    }

    const glm::vec3& getDirection() const {
        assert(type == Type::Directional && "Not a directional light");
        return direction;
    }

    // Metody dla światła punktowego
    void setRadius(float r) {
        assert(type == Type::Point && "Not a point light");
        radius = r;
        incrementVersion();
    }

    float getRadius() const {
        assert(type == Type::Point && "Not a point light");
        return radius;
    }

    // Wspólna metoda dla koloru
    void setColor(const glm::vec4& col) {
        color = col;
        incrementVersion();
    }

    const glm::vec4& getColor() const { return color; }
};