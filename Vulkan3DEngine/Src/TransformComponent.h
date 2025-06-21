#pragma once
#include "Component.h"
#include "BinaryWriter.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct TransformComponent : public Component {
public:
    const char* getName() const override;

    // Global position setters/getters
    void setPosition(const glm::vec3& worldPosition);
    glm::vec3 getPosition() const;

    // Local position setters/getters
    void setLocalPosition(const glm::vec3& localPosition);
    const glm::vec3& getLocalPosition() const;

    // Global rotation setters/getters
    void setRotation(const glm::vec3& worldRotation);
    glm::vec3 getRotation() const;

    // Local rotation setters/getters
    void setLocalRotation(const glm::vec3& localRotation);
    const glm::vec3& getLocalRotation() const;

    // Global scale setters/getters
    void setScale(const glm::vec3& worldScale);
    glm::vec3 getScale() const;

    // Local scale setters/getters
    void setLocalScale(const glm::vec3& localScale);
    const glm::vec3& getLocalScale() const;

    // Matrix calculations
    glm::mat4 getLocalMatrix() const;
    glm::mat4 getWorldMatrix() const;
    glm::mat4 getViewMatrix() const;

    // Transformation methods (operate in global space)
    void translate(const glm::vec3& delta);
    void rotate(const glm::vec3& delta);
    void scale(const glm::vec3& factor);

    // Local transformation methods
    void translateLocal(const glm::vec3& delta);
    void rotateLocal(const glm::vec3& delta);
    void scaleLocal(const glm::vec3& factor);

    // ISerializable implementation
    json serialize() const override;
    void deserialize(const json& j) override;

    void renderUI() override;

private:
    glm::vec3 m_position{ 0.0f };
    glm::vec3 m_rotation{ 0.0f }; // Degrees
    glm::vec3 m_scale{ 1.0f };
};