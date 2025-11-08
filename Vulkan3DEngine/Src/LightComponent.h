#pragma once
#include "Component.h"
#include "BinaryWriter.h"
#include <glm/glm.hpp>
#include <cassert>

struct LightComponent : public Component {
    enum class Type { Directional, Point };

    Type type;
    glm::vec3 color;      // RGB tylko
    float intensity;      // Osobna intensywność

    // Dane specyficzne dla typu światła
    glm::vec3 direction; // Używane tylko dla Type::Directional
    float radius;        // Używane tylko dla Type::Point

    // Default constructor - initializes as a Directional light by default
    LightComponent() : type(Type::Directional) {
        color = glm::vec3(1.0f);
        intensity = 1.0f;
        direction = glm::vec3(0.0f, -1.0f, 0.0f);
        radius = 0.0f;
    }

    explicit LightComponent(Type lightType) : type(lightType) {
        color = glm::vec3(1.0f);
        intensity = 1.0f;
        if (type == Type::Directional) {
            direction = glm::vec3(0.0f, -1.0f, 0.0f);
            radius = 0.0f;
        }
        else {
            direction = glm::vec3(0.0f);
            radius = 10.0f;
        }
    }

    const char* getName() const override {
        return "LightComponent";
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

    void setType(Type newType) {
        if (type != newType) {
            type = newType;
            if (type == Type::Directional) {
                direction = glm::vec3(0.0f, -1.0f, 0.0f);
                radius = 0.0f;
            }
            else {
                direction = glm::vec3(0.0f);
                radius = 10.0f;
            }
            incrementVersion();
        }
    }

    Type getType() const {
        return type;
    }

    // Wspólne metody dla koloru i intensywności
    void setColor(const glm::vec3& col) {
        color = col;
        incrementVersion();
    }

    const glm::vec3& getColor() const {
        return color;
    }

    void setIntensity(float i) {
        intensity = i;
        incrementVersion();
    }

    float getIntensity() const {
        return intensity;
    }

    // Helper do pakowania koloru i intensywności do vec4
    glm::vec4 getColorWithIntensity() const {
        return glm::vec4(color, intensity);
    }

    // ISerializable implementation
    json serialize() const override {
        json j;
        j["type"] = (type == Type::Directional) ? "directional" : "point";
        j["color"] = { color.r, color.g, color.b };
        j["intensity"] = intensity;

        if (type == Type::Directional) {
            j["direction"] = { direction.x, direction.y, direction.z };
        }
        else {
            j["radius"] = radius;
        }

        return j;
    }

    void deserialize(const json& j) override {
        if (j.contains("type") && j["type"].is_string()) {
            std::string typeStr = j["type"];
            type = (typeStr == "directional") ? Type::Directional : Type::Point;
        }

        if (j.contains("color") && j["color"].is_array() && j["color"].size() == 3) {
            color = glm::vec3(j["color"][0], j["color"][1], j["color"][2]);
        }

        if (j.contains("intensity") && j["intensity"].is_number()) {
            intensity = j["intensity"];
        }
        else if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4) {
            intensity = j["color"][3];
        }

        if (type == Type::Directional && j.contains("direction") &&
            j["direction"].is_array() && j["direction"].size() == 3) {
            direction = glm::vec3(j["direction"][0], j["direction"][1], j["direction"][2]);
        }

        if (type == Type::Point && j.contains("radius") && j["radius"].is_number()) {
            radius = j["radius"];
        }

        incrementVersion();
    }

    void renderUI() override {
        ImGui::Text("Light Component");

        // Light type selector
        const char* lightTypes[] = { "Directional", "Point" };
        int currentType = (type == Type::Directional) ? 0 : 1;

        if (ImGui::Combo("Light Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes))) {
            Type newType = (currentType == 0) ? Type::Directional : Type::Point;
            setType(newType);
        }

        // Color picker (RGB tylko)
        glm::vec3 lightColor = getColor();
        if (ImGui::ColorEdit3("Color", &lightColor.r)) {
            setColor(lightColor);
        }

        // Intensity slider
        float lightIntensity = getIntensity();
        if (ImGui::DragFloat("Intensity", &lightIntensity, 0.1f, 0.0f, 10.0f)) {
            setIntensity(lightIntensity);
        }

        // Type-specific properties
        if (type == Type::Directional) {
            ImGui::Text("Direction");
            glm::vec3 dir = getDirection();
            if (ImGui::DragFloat3("##Direction", &dir.x, 0.01f, -1.0f, 1.0f)) {
                setDirection(dir);
            }
        }
        else if (type == Type::Point) {
            ImGui::Text("Radius");
            float r = getRadius();
            if (ImGui::DragFloat("##Radius", &r, 0.1f, 0.1f, 100.0f)) {
                setRadius(r);
            }
        }
    }
};
