#pragma once
#include "Prerequisites.h"
#include "AnimationSequence.h"
#include <iostream>

class SceneObject
{
public:
    SceneObject(Scene* scene, std::string name) : m_position(0.0f), m_rotation(0.0f), m_scale(1.0f), m_startPosition(0.0f),
        m_startRotation(0.0f), m_startScale(1.0f), p_scene(scene), m_name(name) {}
    SceneObject(glm::vec3 position, glm::vec3 rotation, float scale, Scene* scene, std::string name) :
        m_position(position), m_rotation(rotation), m_scale(scale), m_startPosition(position), m_startRotation(rotation),
        m_startScale(scale), p_scene(scene), m_name(name) {}
    SceneObject(const nlohmann::json& j, Scene* scene): p_scene(scene)
    {
        from_json(j);
    }

    ~SceneObject() {
    }

    // Move the object relative to its current position
    void move(float dx, float dy, float dz) {
        m_position += glm::vec3(dx, dy, dz);
    }

    // Rotate the object relative to its current rotation
    void rotate(float pitch, float yaw, float roll) {
        m_rotation += glm::vec3(pitch, yaw, roll);
    }

    void setName(std::string name) {
        m_name = name;
    }

    void setPosition(float x, float y, float z) {
        m_position = glm::vec3(x, y, z);
    }

    void setPosition(glm::vec3 vec) {
        m_position = vec;
    }

    void setRotation(float pitch, float yaw, float roll) {
        m_rotation = glm::vec3(pitch, yaw, roll);
    }

    void setRotation(glm::vec3 vec) {
        m_rotation = vec;
    }

    void setScale(float scale) {
        m_scale = scale;
    }

    void setStartPosition(float x, float y, float z) {
        m_startPosition = glm::vec3(x, y, z);
    }

    void setStartPosition(glm::vec3 vec) {
        m_startPosition = vec;
    }

    void setStartRotation(float pitch, float yaw, float roll) {
        m_startRotation = glm::vec3(pitch, yaw, roll);
    }

    void setStartRotation(glm::vec3 vec) {
        m_startRotation = vec;
    }

    void setStartScale(float scale) {
        m_startScale = scale;
    }

    virtual void update() {
        if (m_animationSequence) {
            m_animationSequence->update();
        }
        positionMatrix = glm::translate(glm::mat4(1.0f), m_position);
        rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(m_rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale));
        //transformMatrix = scaleMatrix * translationMatrix * rotationMatrix;
    }

    virtual void draw() = 0;

    // Getter functions
    std::string getName() const { return m_name; }
    glm::vec3 getPosition() const { return m_position; }
    glm::vec3 getRotation() const { return m_rotation; }
    float getScale() const { return m_scale; }

    glm::vec3 getStartPosition() const { return m_startPosition; }
    glm::vec3 getStartRotation() const { return m_startRotation; }
    float getStartScale() const { return m_startScale; }

    std::shared_ptr<AnimationSequence> getAnimationSequence() const {
        return m_animationSequence;
    }

    void setAnimationSequence(std::shared_ptr<AnimationSequence> sequence) {
        m_animationSequence = sequence;
    }

    bool isActive = true;

    virtual void to_json(nlohmann::json& j) {
        nlohmann::json animSequenceJson;
        if (m_animationSequence) {
            m_animationSequence->to_json(animSequenceJson);
        }
        else {
            animSequenceJson = nullptr;
        }

        j = nlohmann::json{
            {"name", m_name},
            {"position", {m_startPosition.x, m_startPosition.y, m_startPosition.z}},
            {"rotation", {m_startRotation.x, m_startRotation.y, m_startRotation.z}},
            {"scale", m_startScale},
            {"animationSequence", animSequenceJson}
        };
    }

    virtual void from_json(const nlohmann::json& j) {
        try {
            m_name = j.at("name").get<std::string>();
            auto pos = j.at("position");
            m_startPosition = glm::vec3(pos[0], pos[1], pos[2]);
            m_position = m_startPosition;
            auto rot = j.at("rotation");
            m_startRotation = glm::vec3(rot[0], rot[1], rot[2]);
            m_rotation = m_startRotation;
            m_startScale = j.at("scale").get<float>();
            m_scale = m_startScale;

            if (j.contains("animationSequence") && !j["animationSequence"].is_null()) {
                auto animSequence = std::make_shared<AnimationSequence>();
                animSequence->from_json(j.at("animationSequence"), this);
                m_animationSequence = animSequence;
            }
        }
        catch (const std::exception& e) {
            fmt::print(stderr, "Error: Failed to deserialize SceneObject data. Exception: {}\n", e.what());
            throw; // Rzuæmy wyj¹tek dalej, aby ³atwiej by³o œledziæ b³¹d
        }
    }


protected:
    Scene* p_scene;
    std::string m_name;
    glm::vec3 m_position = glm::vec3(0.0f);
    glm::vec3 m_rotation = glm::vec3(0.0f);
    float m_scale;
    glm::vec3 m_startPosition = glm::vec3(0.0f);
    glm::vec3 m_startRotation = glm::vec3(0.0f);
    float m_startScale;

    glm::mat4 transformMatrix = glm::mat4(1.0f);
    glm::mat4 positionMatrix = glm::mat4(1.0f);
    glm::mat4 rotationMatrix = glm::mat4(1.0f);
    glm::mat4 scaleMatrix = glm::mat4(1.0f);

    std::shared_ptr<AnimationSequence> m_animationSequence;

    friend class SceneObjectManager;
};


