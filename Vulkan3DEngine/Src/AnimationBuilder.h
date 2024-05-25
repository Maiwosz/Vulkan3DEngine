#pragma once
#include "Prerequisites.h"
#include <functional>
#include <vector>
#include <memory>
#include <condition_variable>
#include <atomic>
#include "Animation.h"

class MoveAnimationData : public AnimationData {
public:
    glm::vec3 startPosition;
    glm::vec3 endPosition;

    void to_json(nlohmann::json& j) const {
        j = nlohmann::json{
            {"type", "move"},
            {"startPosition", {startPosition.x, startPosition.y, startPosition.z}},
            {"endPosition", {endPosition.x, endPosition.y, endPosition.z}}
        };
    }

    void from_json(const nlohmann::json& j) {
        try {
            auto sp = j.at("startPosition");
            startPosition = glm::vec3(sp[0], sp[1], sp[2]);
            auto ep = j.at("endPosition");
            endPosition = glm::vec3(ep[0], ep[1], ep[2]);
            fmt::print("MoveAnimationData initialized successfully\n");
        }
        catch (const std::exception& e) {
            fmt::print(stderr, "Error: Failed to deserialize MoveAnimationData. Exception: {}\n", e.what());
            throw;
        }
    }
};

class RotateAnimationData : public AnimationData {
public:
    glm::vec3 startRotation;
    glm::vec3 endRotation;

    void to_json(nlohmann::json& j) const {
        j = nlohmann::json{
            {"type", "rotate"},
            {"startRotation", {startRotation.x, startRotation.y, startRotation.z}},
            {"endRotation", {endRotation.x, endRotation.y, endRotation.z}}
        };
    }

    void from_json(const nlohmann::json& j) {
        try {
            auto sr = j.at("startRotation");
            startRotation = glm::vec3(sr[0], sr[1], sr[2]);
            auto er = j.at("endRotation");
            endRotation = glm::vec3(er[0], er[1], er[2]);
            fmt::print("RotateAnimationData initialized successfully\n");
        }
        catch (const std::exception& e) {
            fmt::print(stderr, "Error: Failed to deserialize RotateAnimationData. Exception: {}\n", e.what());
            throw;
        }
    }
};

class OrbitAnimationData : public AnimationData {
public:
    glm::vec3 center;
    float radius;
    float angularSpeed;
    glm::vec3 axis;
    float phaseShift;

    void to_json(nlohmann::json& j) const {
        j = nlohmann::json{
            {"type", "orbit"},
            {"center", {center.x, center.y, center.z}},
            {"radius", radius},
            {"angularSpeed", angularSpeed},
            {"axis", {axis.x, axis.y, axis.z}},
            {"phaseShift", phaseShift}
        };
    }

    void from_json(const nlohmann::json& j) {
        try {
            auto c = j.at("center");
            center = glm::vec3(c[0], c[1], c[2]);
            radius = j.at("radius");
            angularSpeed = j.at("angularSpeed");
            auto a = j.at("axis");
            axis = glm::vec3(a[0], a[1], a[2]);
            phaseShift = j.at("phaseShift");
            fmt::print("OrbitAnimationData initialized successfully\n");
        }
        catch (const std::exception& e) {
            fmt::print(stderr, "Error: Failed to deserialize OrbitAnimationData. Exception: {}\n", e.what());
            throw;
        }
    }
};

class WaitAnimationData : public AnimationData {
public:

    void to_json(nlohmann::json& j) const {
        j = nlohmann::json{
            {"type", "wait"},
        };
    }

    void from_json(const nlohmann::json& j) {
        try {

        }
        catch (const std::exception& e) {
            fmt::print(stderr, "Error: Failed to deserialize WaitAnimationData. Exception: {}\n", e.what());
            throw;
        }
    }
};

class AnimationBuilder {
public:
    AnimationBuilder& move(SceneObject* object, glm::vec3 startPosition, glm::vec3 endPosition, float duration);
    AnimationBuilder& rotate(SceneObject* object, glm::vec3 startRotation, glm::vec3 endRotation, float duration);
    AnimationBuilder& orbit(SceneObject* object, glm::vec3 center, float radius, float angularSpeed, float duration, glm::vec3 axis, float phaseShift = 0.0f);
    AnimationBuilder& custom(std::function<void(float)> customFunc, float duration);
    AnimationBuilder& wait(SceneObject* object, float duration);
    std::shared_ptr<AnimationSequence> build();
    void build(AnimationSequence& sequence);

private:
    std::vector<Animation> animations;
};