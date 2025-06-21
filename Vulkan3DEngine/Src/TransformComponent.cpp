#include "TransformComponent.h"
#include "Registry.h"

const char* TransformComponent::getName() const {
    return "TransformComponent";
}

void TransformComponent::setPosition(const glm::vec3& worldPosition) {
    if (!getRegistry()) {
        SPDLOG_ERROR("Registry is null in TransformComponent::setPosition for entity {}", entity.id);
    }

    if (!getRegistry() || !getRegistry()->entities().hasParent(entity)) {
        m_position = worldPosition;
    }
    else {
        Entity parent = getRegistry()->entities().getParent(entity);
        if (getRegistry()->components().hasComponent<TransformComponent>(parent)) {
            const TransformComponent& parentTransform = getRegistry()->components().getComponent<TransformComponent>(parent);
            glm::mat4 parentWorldMatrix = parentTransform.getWorldMatrix();
            glm::mat4 inverseParentMatrix = glm::inverse(parentWorldMatrix);
            glm::vec4 localPos = inverseParentMatrix * glm::vec4(worldPosition, 1.0f);
            m_position = glm::vec3(localPos);
        }
        else {
            m_position = worldPosition;
        }
    }
    incrementVersion();
}

glm::vec3 TransformComponent::getPosition() const {
    glm::mat4 worldMatrix = getWorldMatrix();
    return glm::vec3(worldMatrix[3]);
}

void TransformComponent::setLocalPosition(const glm::vec3& localPosition) {
    m_position = localPosition;
    incrementVersion();
}

const glm::vec3& TransformComponent::getLocalPosition() const {
    return m_position;
}

void TransformComponent::setRotation(const glm::vec3& worldRotation) {
    if (!getRegistry() || !getRegistry()->entities().hasParent(entity)) {
        m_rotation = worldRotation;
    }
    else {
        Entity parent = getRegistry()->entities().getParent(entity);
        if (getRegistry()->components().hasComponent<TransformComponent>(parent)) {
            const TransformComponent& parentTransform = getRegistry()->components().getComponent<TransformComponent>(parent);
            glm::vec3 parentWorldRotation = parentTransform.getRotation();
            m_rotation = worldRotation - parentWorldRotation;
        }
        else {
            m_rotation = worldRotation;
        }
    }
    incrementVersion();
}

glm::vec3 TransformComponent::getRotation() const {
    if (!getRegistry() || !getRegistry()->entities().hasParent(entity)) {
        return m_rotation;
    }

    Entity parent = getRegistry()->entities().getParent(entity);
    if (getRegistry()->components().hasComponent<TransformComponent>(parent)) {
        const TransformComponent& parentTransform = getRegistry()->components().getComponent<TransformComponent>(parent);
        return parentTransform.getRotation() + m_rotation;
    }

    return m_rotation;
}

void TransformComponent::setLocalRotation(const glm::vec3& localRotation) {
    m_rotation = localRotation;
    incrementVersion();
}

const glm::vec3& TransformComponent::getLocalRotation() const {
    return m_rotation;
}

void TransformComponent::setScale(const glm::vec3& worldScale) {
    if (!getRegistry() || !getRegistry()->entities().hasParent(entity)) {
        m_scale = worldScale;
    }
    else {
        Entity parent = getRegistry()->entities().getParent(entity);
        if (getRegistry()->components().hasComponent<TransformComponent>(parent)) {
            const TransformComponent& parentTransform = getRegistry()->components().getComponent<TransformComponent>(parent);
            glm::vec3 parentWorldScale = parentTransform.getScale();
            m_scale = worldScale / parentWorldScale;
        }
        else {
            m_scale = worldScale;
        }
    }
    incrementVersion();
}

glm::vec3 TransformComponent::getScale() const {
    glm::mat4 worldMatrix = getWorldMatrix();
    return glm::vec3(
        glm::length(glm::vec3(worldMatrix[0])),
        glm::length(glm::vec3(worldMatrix[1])),
        glm::length(glm::vec3(worldMatrix[2]))
    );
}

void TransformComponent::setLocalScale(const glm::vec3& localScale) {
    m_scale = localScale;
    incrementVersion();
}

const glm::vec3& TransformComponent::getLocalScale() const {
    return m_scale;
}

glm::mat4 TransformComponent::getLocalMatrix() const {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), m_position);

    glm::quat quaternion = glm::quat(glm::vec3(
        glm::radians(m_rotation.x),
        glm::radians(m_rotation.y),
        glm::radians(m_rotation.z)
    ));

    model = model * glm::toMat4(quaternion);
    model = glm::scale(model, m_scale);

    return model;
}

glm::mat4 TransformComponent::getWorldMatrix() const {
    if (!getRegistry()) {
        return getLocalMatrix();
    }

    Registry* registry = getRegistry();
    if (!registry->entities().hasParent(entity)) {
        return getLocalMatrix();
    }

    Entity parent = registry->entities().getParent(entity);
    if (registry->components().hasComponent<TransformComponent>(parent)) {
        const TransformComponent& parentTransform = registry->components().getComponent<TransformComponent>(parent);
        return parentTransform.getWorldMatrix() * getLocalMatrix();
    }

    return getLocalMatrix();
}

glm::mat4 TransformComponent::getViewMatrix() const {
    glm::mat4 worldMatrix = getWorldMatrix();
    glm::vec3 worldPosition = glm::vec3(worldMatrix[3]);

    glm::mat3 rotationMatrix = glm::mat3(worldMatrix);
    glm::vec3 right = glm::normalize(rotationMatrix[0]);
    glm::vec3 up = glm::normalize(rotationMatrix[1]);
    glm::vec3 forward = -glm::normalize(rotationMatrix[2]);

    glm::vec3 target = worldPosition + forward;
    return glm::lookAt(worldPosition, target, up);
}

void TransformComponent::translate(const glm::vec3& delta) {
    setPosition(getPosition() + delta);
}

void TransformComponent::rotate(const glm::vec3& delta) {
    setRotation(getRotation() + delta);
}

void TransformComponent::scale(const glm::vec3& factor) {
    setScale(getScale() * factor);
}

void TransformComponent::translateLocal(const glm::vec3& delta) {
    m_position += delta;
    incrementVersion();
}

void TransformComponent::rotateLocal(const glm::vec3& delta) {
    m_rotation += delta;
    incrementVersion();
}

void TransformComponent::scaleLocal(const glm::vec3& factor) {
    m_scale *= factor;
    incrementVersion();
}

json TransformComponent::serialize() const {
    json j;
    j["position"] = { m_position.x, m_position.y, m_position.z };
    j["rotation"] = { m_rotation.x, m_rotation.y, m_rotation.z };
    j["scale"] = { m_scale.x, m_scale.y, m_scale.z };
    return j;
}

void TransformComponent::deserialize(const json& j) {
    if (j.contains("position") && j["position"].is_array() && j["position"].size() == 3) {
        m_position = glm::vec3(j["position"][0], j["position"][1], j["position"][2]);
    }
    if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() == 3) {
        m_rotation = glm::vec3(j["rotation"][0], j["rotation"][1], j["rotation"][2]);
    }
    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() == 3) {
        m_scale = glm::vec3(j["scale"][0], j["scale"][1], j["scale"][2]);
    }
    incrementVersion();
}

void TransformComponent::renderUI() {
    bool hasParent = getRegistry() && getRegistry()->entities().hasParent(entity);

    // Position
    ImGui::Text("Position");
    glm::vec3 position = getLocalPosition();
    if (ImGui::DragFloat3("##Position", &position.x, 0.1f)) {
        setLocalPosition(position);
    }

    if (hasParent) {
        ImGui::SameLine();
        ImGui::TextDisabled("(Local)");

        glm::vec3 worldPos = getPosition();
        ImGui::Text("World Position: %.2f, %.2f, %.2f", worldPos.x, worldPos.y, worldPos.z);
    }

    // Rotation
    ImGui::Text("Rotation");
    glm::vec3 rotation = getLocalRotation();
    if (ImGui::DragFloat3("##Rotation", &rotation.x, 1.0f)) {
        setLocalRotation(rotation);
    }

    if (hasParent) {
        ImGui::SameLine();
        ImGui::TextDisabled("(Local)");

        glm::vec3 worldRot = getRotation();
        ImGui::Text("World Rotation: %.2f, %.2f, %.2f", worldRot.x, worldRot.y, worldRot.z);
    }

    // Scale
    ImGui::Text("Scale");
    glm::vec3 scale = getLocalScale();
    if (ImGui::DragFloat3("##Scale", &scale.x, 0.01f, 0.001f)) {
        setLocalScale(scale);
    }

    if (hasParent) {
        ImGui::SameLine();
        ImGui::TextDisabled("(Local)");

        glm::vec3 worldScale = getScale();
        ImGui::Text("World Scale: %.2f, %.2f, %.2f", worldScale.x, worldScale.y, worldScale.z);
    }
}
