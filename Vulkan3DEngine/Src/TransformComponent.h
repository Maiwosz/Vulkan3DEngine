#pragma once
#include "Component.h"
#include "Registry.h"
#include "BinaryWriter.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct TransformComponent : public Component {
public:
    // Global position setters/getters
    void setPosition(const glm::vec3& worldPosition) {
        if (!getRegistry() || !getRegistry()->hasParent(entity)) {
            m_position = worldPosition;
        }
        else {
            Entity parent = getRegistry()->getParent(entity);
            if (getRegistry()->hasComponent<TransformComponent>(parent)) {
                const TransformComponent& parentTransform = getRegistry()->getComponent<TransformComponent>(parent);
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

    glm::vec3 getPosition() const {
        glm::mat4 worldMatrix = getWorldMatrix();
        return glm::vec3(worldMatrix[3]);
    }

    // Local position setters/getters
    void setLocalPosition(const glm::vec3& localPosition) {
        m_position = localPosition;
        incrementVersion();
    }

    const glm::vec3& getLocalPosition() const {
        return m_position;
    }

    // Global rotation setters/getters
    void setRotation(const glm::vec3& worldRotation) {
        if (!getRegistry() || !getRegistry()->hasParent(entity)) {
            m_rotation = worldRotation;
        }
        else {
            Entity parent = getRegistry()->getParent(entity);
            if (getRegistry()->hasComponent<TransformComponent>(parent)) {
                const TransformComponent& parentTransform = getRegistry()->getComponent<TransformComponent>(parent);
                glm::vec3 parentWorldRotation = parentTransform.getRotation();
                m_rotation = worldRotation - parentWorldRotation;
            }
            else {
                m_rotation = worldRotation;
            }
        }
        incrementVersion();
    }

    glm::vec3 getRotation() const {
        if (!getRegistry() || !getRegistry()->hasParent(entity)) {
            return m_rotation;
        }

        Entity parent = getRegistry()->getParent(entity);
        if (getRegistry()->hasComponent<TransformComponent>(parent)) {
            const TransformComponent& parentTransform = getRegistry()->getComponent<TransformComponent>(parent);
            return parentTransform.getRotation() + m_rotation;
        }

        return m_rotation;
    }

    // Local rotation setters/getters
    void setLocalRotation(const glm::vec3& localRotation) {
        m_rotation = localRotation;
        incrementVersion();
    }

    const glm::vec3& getLocalRotation() const {
        return m_rotation;
    }

    // Global scale setters/getters
    void setScale(const glm::vec3& worldScale) {
        if (!getRegistry() || !getRegistry()->hasParent(entity)) {
            m_scale = worldScale;
        }
        else {
            Entity parent = getRegistry()->getParent(entity);
            if (getRegistry()->hasComponent<TransformComponent>(parent)) {
                const TransformComponent& parentTransform = getRegistry()->getComponent<TransformComponent>(parent);
                glm::vec3 parentWorldScale = parentTransform.getScale();
                m_scale = worldScale / parentWorldScale;
            }
            else {
                m_scale = worldScale;
            }
        }
        incrementVersion();
    }

    glm::vec3 getScale() const {
        glm::mat4 worldMatrix = getWorldMatrix();
        return glm::vec3(
            glm::length(glm::vec3(worldMatrix[0])),
            glm::length(glm::vec3(worldMatrix[1])),
            glm::length(glm::vec3(worldMatrix[2]))
        );
    }

    // Local scale setters/getters
    void setLocalScale(const glm::vec3& localScale) {
        m_scale = localScale;
        incrementVersion();
    }

    const glm::vec3& getLocalScale() const {
        return m_scale;
    }

    // Matrix calculations
    glm::mat4 getLocalMatrix() const {
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

    glm::mat4 getWorldMatrix() const {
        if (!getRegistry()) {
            return getLocalMatrix();
        }

        Registry* registry = getRegistry();
        if (!registry->hasParent(entity)) {
            return getLocalMatrix();
        }

        Entity parent = registry->getParent(entity);
        if (registry->hasComponent<TransformComponent>(parent)) {
            const TransformComponent& parentTransform = registry->getComponent<TransformComponent>(parent);
            return parentTransform.getWorldMatrix() * getLocalMatrix();
        }

        return getLocalMatrix();
    }

    glm::mat4 getViewMatrix() const {
        glm::mat4 worldMatrix = getWorldMatrix();
        glm::vec3 worldPosition = glm::vec3(worldMatrix[3]);

        glm::mat3 rotationMatrix = glm::mat3(worldMatrix);
        glm::vec3 right = glm::normalize(rotationMatrix[0]);
        glm::vec3 up = glm::normalize(rotationMatrix[1]);
        glm::vec3 forward = -glm::normalize(rotationMatrix[2]);

        glm::vec3 target = worldPosition + forward;
        return glm::lookAt(worldPosition, target, up);
    }

    // Transformation methods (operate in global space)
    void translate(const glm::vec3& delta) {
        setPosition(getPosition() + delta);
    }

    void rotate(const glm::vec3& delta) {
        setRotation(getRotation() + delta);
    }

    void scale(const glm::vec3& factor) {
        setScale(getScale() * factor);
    }

    // Local transformation methods
    void translateLocal(const glm::vec3& delta) {
        m_position += delta;
        incrementVersion();
    }

    void rotateLocal(const glm::vec3& delta) {
        m_rotation += delta;
        incrementVersion();
    }

    void scaleLocal(const glm::vec3& factor) {
        m_scale *= factor;
        incrementVersion();
    }

    // ISerializable implementation
    json serialize() const override {
        json j;
        j["position"] = { m_position.x, m_position.y, m_position.z };
        j["rotation"] = { m_rotation.x, m_rotation.y, m_rotation.z };
        j["scale"] = { m_scale.x, m_scale.y, m_scale.z };
        return j;
    }

    void deserialize(const json& j) override {
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

    // IBinarySerializable implementation
    std::vector<uint8_t> serializeBinary() const override {
        BinaryWriter writer;

        // Write position, rotation, and scale
        writer.write(m_position);
        writer.write(m_rotation);
        writer.write(m_scale);

        return writer.getData();
    }

    size_t deserializeBinary(const uint8_t* data, size_t size) override {
        BinaryReader reader(data, size);

        if (!reader.read(m_position)) return 0;
        if (!reader.read(m_rotation)) return 0;
        if (!reader.read(m_scale)) return 0;

        incrementVersion();
        return reader.getPosition();
    }

private:
    glm::vec3 m_position{ 0.0f };
    glm::vec3 m_rotation{ 0.0f }; // Degrees
    glm::vec3 m_scale{ 1.0f };
};