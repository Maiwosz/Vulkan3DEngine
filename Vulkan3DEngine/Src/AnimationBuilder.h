#pragma once
#include "Prerequisites.h"
#include <functional>
#include <vector>
#include <memory>
#include <condition_variable>
#include <atomic>

class AnimationBuilder {
public:
    AnimationBuilder& move(SceneObject* object, glm::vec3 startPosition, glm::vec3 endPosition, float duration);
    AnimationBuilder& rotate(SceneObject* object, glm::vec3 startRotation, glm::vec3 endRotation, float duration);
    AnimationBuilder& orbit(SceneObject* object, glm::vec3 center, float radius, float angularSpeed, float duration, glm::vec3 axis, float phaseShift = 0.0f);
    AnimationBuilder& custom(std::function<void(float)> customFunc, float duration);
    AnimationBuilder& wait(float duration);
    std::shared_ptr<AnimationSequence> build();

private:
    std::vector<Animation> animations;
};