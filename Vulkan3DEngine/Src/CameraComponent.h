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

    void setRenderTarget(const RenderTarget& target) {
        m_renderTarget = target;
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
    const RenderTarget& getRenderTarget() const { return m_renderTarget; }

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

        // Serialize render target
        j["renderTargetType"] = (m_renderTarget.isSwapchain()) ? "swapchain" : "texture";
        if (m_renderTarget.isTexture()) {
            j["renderTargetHandle"] = m_renderTarget.getTextureHandle().handle().id;
        }

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

        // Note: Render target deserialization is complex and may need special handling
        // For now, we'll default to swapchain - texture handles would need to be resolved
        // through the asset system during a separate post-deserialization phase

        incrementVersion();
    }

    void renderUI() override {
        ImGui::Text("Camera Component");

        // Projection type selector
        const char* projectionTypes[] = { "Perspective", "Orthographic" };
        int currentProjection = (m_projectionType == ProjectionType::Perspective) ? 0 : 1;

        if (ImGui::Combo("Projection", &currentProjection, projectionTypes, IM_ARRAYSIZE(projectionTypes))) {
            ProjectionType newType = (currentProjection == 0) ? ProjectionType::Perspective : ProjectionType::Orthographic;
            setProjectionType(newType);
        }

        // Aspect ratio
        float aspectRatio = getAspectRatio();
        if (ImGui::DragFloat("Aspect Ratio", &aspectRatio, 0.01f, 0.1f, 10.0f)) {
            setAspectRatio(aspectRatio);
        }

        // Projection-specific settings
        if (m_projectionType == ProjectionType::Perspective) {
            float fov = getVerticalFOV();
            if (ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f, "%.1f°")) {
                setVerticalFOV(fov);
            }
        }
        else {
            float orthoSize = getOrthographicSize();
            if (ImGui::DragFloat("Orthographic Size", &orthoSize, 0.1f, 0.1f, 100.0f)) {
                setOrthographicSize(orthoSize);
            }
        }

        // Clipping planes
        ImGui::Text("Clipping Planes");
        float nearClip = getNearClip();
        float farClip = getFarClip();

        if (ImGui::DragFloat("Near", &nearClip, 0.01f, 0.001f, 10.0f)) {
            setClippingPlanes(nearClip, farClip);
        }

        if (ImGui::DragFloat("Far", &farClip, 1.0f, nearClip + 1.0f, 10000.0f)) {
            setClippingPlanes(nearClip, farClip);
        }

        // Render target info (read-only for now)
        ImGui::Separator();
        ImGui::Text("Render Target");
        if (m_renderTarget.isSwapchain()) {
            ImGui::Text("Type: Swapchain");
        }
        else {
            ImGui::Text("Type: Texture");
            ImGui::Text("Handle ID: %u", m_renderTarget.getTextureHandle().handle().id);
        }
    }

private:
    ProjectionType m_projectionType = ProjectionType::Perspective;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_verticalFOV = 60.0f;
    float m_orthographicSize = 10.0f;
    float m_nearClip = 0.1f;
    float m_farClip = 1000.0f;
    RenderTarget m_renderTarget = RenderTarget::createSwapChainTarget();
};