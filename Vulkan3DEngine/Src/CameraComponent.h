#pragma once
#include "Component.h"
#include "BinaryWriter.h"
#include <glm/glm.hpp>

struct TransformComponent;

struct CameraComponent : public Component {
public:
    enum class ProjectionType {
        Perspective,
        Orthographic
    };

    // Setters
    void setProjectionType(ProjectionType type) {
        m_projectionType = type;
        incrementVersion();
    }

    void setAspectRatio(float ratio) {
        m_aspectRatio = ratio;
        incrementVersion();
    }

    void setVerticalFOV(float fov) {
        m_verticalFOV = fov;
        incrementVersion();
    }

    void setOrthographicSize(float size) {
        m_orthographicSize = size;
        incrementVersion();
    }

    void setClippingPlanes(float nearPlane, float farPlane) {
        m_nearClip = nearPlane;
        m_farClip = farPlane;
        incrementVersion();
    }

    // Getters
    const char* getName() const override {
        return "CameraComponent";
    }
    ProjectionType getProjectionType() const { return m_projectionType; }
    float getAspectRatio() const { return m_aspectRatio; }
    float getVerticalFOV() const { return m_verticalFOV; }
    float getOrthographicSize() const { return m_orthographicSize; }
    float getNearClip() const { return m_nearClip; }
    float getFarClip() const { return m_farClip; }

    // Helper functions
    glm::mat4 getProjectionMatrix() const {
        if (m_projectionType == ProjectionType::Perspective) {
            return glm::perspective(
                glm::radians(m_verticalFOV),
                m_aspectRatio,
                m_nearClip,
                m_farClip
            );
        }
        float halfWidth = m_orthographicSize * m_aspectRatio;
        return glm::ortho(
            -halfWidth, halfWidth,
            -m_orthographicSize, m_orthographicSize,
            m_nearClip, m_farClip
        );
    }

    // ISerializable implementation
    json serialize() const override {
        json j;
        j["projectionType"] = (m_projectionType == ProjectionType::Perspective) ? "perspective" : "orthographic";
        j["aspectRatio"] = m_aspectRatio;
        j["verticalFOV"] = m_verticalFOV;
        j["orthographicSize"] = m_orthographicSize;
        j["nearClip"] = m_nearClip;
        j["farClip"] = m_farClip;
        return j;
    }

    void deserialize(const json& j) override {
        if (j.contains("projectionType") && j["projectionType"].is_string()) {
            std::string projType = j["projectionType"];
            m_projectionType = (projType == "perspective") ? ProjectionType::Perspective : ProjectionType::Orthographic;
        }
        if (j.contains("aspectRatio") && j["aspectRatio"].is_number()) {
            m_aspectRatio = j["aspectRatio"];
        }
        if (j.contains("verticalFOV") && j["verticalFOV"].is_number()) {
            m_verticalFOV = j["verticalFOV"];
        }
        if (j.contains("orthographicSize") && j["orthographicSize"].is_number()) {
            m_orthographicSize = j["orthographicSize"];
        }
        if (j.contains("nearClip") && j["nearClip"].is_number()) {
            m_nearClip = j["nearClip"];
        }
        if (j.contains("farClip") && j["farClip"].is_number()) {
            m_farClip = j["farClip"];
        }
        incrementVersion();
    }

private:
    ProjectionType m_projectionType = ProjectionType::Perspective;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_verticalFOV = 60.0f;
    float m_orthographicSize = 10.0f;
    float m_nearClip = 0.1f;
    float m_farClip = 1000.0f;
};