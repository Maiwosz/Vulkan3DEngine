#include "CameraComponent.h"
#include <glm/gtc/matrix_transform.hpp>
#include "Engine.h"

// Setters
void CameraComponent::setProjectionType(ProjectionType type) {
    m_projectionType = type;
    incrementVersion();
}

void CameraComponent::setAspectRatio(float ratio) {
    m_aspectRatio = ratio;
    incrementVersion();
}

void CameraComponent::setVerticalFOV(float fov) {
    m_verticalFOV = fov;
    incrementVersion();
}

void CameraComponent::setOrthographicSize(float size) {
    m_orthographicSize = size;
    incrementVersion();
}

void CameraComponent::setClippingPlanes(float nearPlane, float farPlane) {
    m_nearClip = nearPlane;
    m_farClip = farPlane;
    incrementVersion();
}

void CameraComponent::setRenderTarget(const RenderTarget& target) {
    m_renderTarget = target;
    incrementVersion();
}

// Getters
const char* CameraComponent::getName() const {
    return "CameraComponent";
}

// Helper functions
glm::mat4 CameraComponent::getProjectionMatrix() const {
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
json CameraComponent::serialize() const {
    json j;
    j["projectionType"] = (m_projectionType == ProjectionType::Perspective) ? "perspective" : "orthographic";
    j["aspectRatio"] = m_aspectRatio;
    j["verticalFOV"] = m_verticalFOV;
    j["orthographicSize"] = m_orthographicSize;
    j["nearClip"] = m_nearClip;
    j["farClip"] = m_farClip;

    // Serialize render target
    if (m_renderTarget.isInvalid()) {
        j["renderTargetType"] = "invalid";
    }
    else if (m_renderTarget.isSwapchain()) {
        j["renderTargetType"] = "swapchain";
    }
    else {
        j["renderTargetType"] = "texture";
        j["renderTargetHandle"] = m_renderTarget.getTextureHandle().handle().id;
    }

    return j;
}

void CameraComponent::deserialize(const json& j) {
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

    m_renderTarget = RenderTarget::createSwapChainTarget(&getEngine()->engineCore().swapChain());

    // Note: Render target deserialization is complex and may need special handling
    // For now, we'll default to invalid - texture handles would need to be resolved
    // through the asset system during a separate post-deserialization phase
    // SwapChain target should be set by the system after deserialization

    incrementVersion();
}

void CameraComponent::renderUI() {
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
    if (m_renderTarget.isInvalid()) {
        ImGui::Text("Type: Invalid");
    }
    else if (m_renderTarget.isSwapchain()) {
        ImGui::Text("Type: Swapchain");
    }
    else {
        ImGui::Text("Type: Texture");
        ImGui::Text("Handle ID: %u", m_renderTarget.getTextureHandle().handle().id);
    }
}