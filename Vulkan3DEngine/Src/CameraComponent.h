#pragma once
#include "Component.h"
#include "BinaryWriter.h"
#include "RenderTarget.h"
#include <glm/glm.hpp>

struct TransformComponent;

struct CameraComponent : public Component {
public:
    enum class ProjectionType {
        Perspective,
        Orthographic
    };


    // Setters
    void setProjectionType(ProjectionType type);
    void setAspectRatio(float ratio);
    void setVerticalFOV(float fov);
    void setOrthographicSize(float size);
    void setClippingPlanes(float nearPlane, float farPlane);
    void setRenderTarget(const RenderTarget& target);

    // Getters
    const char* getName() const override;
    ProjectionType getProjectionType() const { return m_projectionType; }
    float getAspectRatio() const { return m_aspectRatio; }
    float getVerticalFOV() const { return m_verticalFOV; }
    float getOrthographicSize() const { return m_orthographicSize; }
    float getNearClip() const { return m_nearClip; }
    float getFarClip() const { return m_farClip; }
    const RenderTarget& getRenderTarget() const { return m_renderTarget; }

    // Helper functions
    glm::mat4 getProjectionMatrix() const;

    // ISerializable implementation
    json serialize() const override;
    void deserialize(const json& j) override;
    void renderUI() override;

private:
    ProjectionType m_projectionType = ProjectionType::Perspective;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_verticalFOV = 60.0f;
    float m_orthographicSize = 10.0f;
    float m_nearClip = 0.1f;
    float m_farClip = 1000.0f;
    RenderTarget m_renderTarget; // Default constructed as Invalid
};