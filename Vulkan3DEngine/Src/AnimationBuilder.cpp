#include "AnimationBuilder.h"
#include "SceneObject.h"
#include "AnimationSequence.h"
#include "Animation.h"
#include <cmath>
#include <corecrt_math_defines.h>

AnimationBuilder& AnimationBuilder::move(SceneObject* object, glm::vec3 startPosition, glm::vec3 endPosition, float duration) {
    std::function<void(float)> moveFunc = [object, startPosition, endPosition](float t) {
        glm::vec3 newPosition = glm::mix(startPosition, endPosition, t);
        object->setPosition(newPosition);
        };
    animations.emplace_back(moveFunc, duration);
    return *this;
}

AnimationBuilder& AnimationBuilder::rotate(SceneObject* object, glm::vec3 startRotation, glm::vec3 endRotation, float duration) {
    std::function<void(float)> rotateFunc = [object, startRotation, endRotation](float t) {
        glm::vec3 newRotation = glm::mix(startRotation, endRotation, t);
        object->setRotation(newRotation);
        };
    animations.emplace_back(rotateFunc, duration);
    return *this;
}

AnimationBuilder& AnimationBuilder::orbit(SceneObject* object, glm::vec3 center, float radius, float angularSpeed, float duration, glm::vec3 axis, float phaseShift) {
    std::function<void(float)> orbitFunc = [object, center, radius, angularSpeed, duration, axis, phaseShift](float t) {
        float angle = glm::radians(angularSpeed) * duration * t + glm::radians(phaseShift);

        glm::vec3 newPosition;
        if (axis == glm::vec3(1.0f, 0.0f, 0.0f)) { // rotation around X axis
            newPosition.x = center.x;
            newPosition.y = center.y + radius * cos(angle);
            newPosition.z = center.z + radius * sin(angle);
        }
        else if (axis == glm::vec3(0.0f, 1.0f, 0.0f)) { // rotation around Y axis
            newPosition.x = center.x + radius * cos(angle);
            newPosition.y = center.y;
            newPosition.z = center.z + radius * sin(angle);
        }
        else if (axis == glm::vec3(0.0f, 0.0f, 1.0f)) { // rotation around Z axis
            newPosition.x = center.x + radius * cos(angle);
            newPosition.y = center.y + radius * sin(angle);
            newPosition.z = center.z;
        }
        else {
            // Default rotation around Y axis
            newPosition.x = center.x + radius * cos(angle);
            newPosition.y = center.y;
            newPosition.z = center.z + radius * sin(angle);
        }

        object->setPosition(newPosition);
        };

    animations.emplace_back(orbitFunc, duration);
    return *this;
}

AnimationBuilder& AnimationBuilder::custom(std::function<void(float)> customFunc, float duration) {
    animations.emplace_back(customFunc, duration);
    return *this;
}

AnimationBuilder& AnimationBuilder::wait(float duration) {
    std::function<void(float)> waitFunc = [](float) {
        // Do nothing
        };
    animations.emplace_back(waitFunc, duration);
    return *this;
}

std::shared_ptr<AnimationSequence> AnimationBuilder::build() {
    auto sequence = std::make_shared<AnimationSequence>();
    for (auto& anim : animations) {
        sequence->addAnimation(anim);
    }
    return sequence;
}