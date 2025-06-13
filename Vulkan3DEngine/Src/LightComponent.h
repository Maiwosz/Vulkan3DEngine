#pragma once
#include "Component.h"
#include "BinaryWriter.h"
#include <glm/glm.hpp>
#include <cassert>

struct LightComponent : public Component {
    enum class Type { Directional, Point };

    Type type;
    glm::vec4 color;

    // Dane specyficzne dla typu światła
    glm::vec3 direction; // Używane tylko dla Type::Directional
    float radius;        // Używane tylko dla Type::Point

    // Default constructor - initializes as a Directional light by default
    LightComponent() : type(Type::Directional) {
        color = glm::vec4(1.0f);
        direction = glm::vec3(0.0f, -1.0f, 0.0f);
        radius = 0.0f; // Nieużywane dla światła kierunkowego
    }

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

    // Method to change light type if needed
    void setType(Type newType) {
        if (type != newType) {
            type = newType;
            // Reset properties based on new type
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

    // Wspólna metoda dla koloru
    void setColor(const glm::vec4& col) {
        color = col;
        incrementVersion();
    }

    const glm::vec4& getColor() const {
        return color;
    }

    // ISerializable implementation
    json serialize() const override {
        json j;
        j["type"] = (type == Type::Directional) ? "directional" : "point";
        j["color"] = { color.r, color.g, color.b, color.a };

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

        if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4) {
            color = glm::vec4(j["color"][0], j["color"][1], j["color"][2], j["color"][3]);
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

    // IBinarySerializable implementation
    std::vector<uint8_t> serializeBinary() const override {
        BinaryWriter writer;

        // Write light type
        writer.write(static_cast<uint8_t>(type));

        // Write color (vec4)
        writer.write(color);

        // Write type-specific data
        if (type == Type::Directional) {
            writer.write(direction);
        }
        else {
            writer.write(radius);
        }

        return writer.getData();
    }

    size_t deserializeBinary(const uint8_t* data, size_t size) override {
        BinaryReader reader(data, size);

        uint8_t lightType;
        if (!reader.read(lightType)) return 0;
        type = static_cast<Type>(lightType);

        if (!reader.read(color)) return 0;

        // Read type-specific data
        if (type == Type::Directional) {
            if (!reader.read(direction)) return 0;
            radius = 0.0f; // Reset unused field
        }
        else {
            if (!reader.read(radius)) return 0;
            direction = glm::vec3(0.0f); // Reset unused field
        }

        incrementVersion();
        return reader.getPosition();
    }
};