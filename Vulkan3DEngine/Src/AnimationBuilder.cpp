#include "AnimationBuilder.h"
#include "SceneObject.h"
#include "AnimationSequence.h"
#include "Animation.h"
#include <cmath>
#include <corecrt_math_defines.h>

AnimationBuilder& AnimationBuilder::move(SceneObject* object, glm::vec3 startPosition, glm::vec3 endPosition, float duration) {
    auto data = std::make_shared<MoveAnimationData>();
    data->startPosition = startPosition;
    data->endPosition = endPosition;
    std::function<void(float)> moveFunc = [object, data](float t) {
        glm::vec3 newPosition = glm::mix(data->startPosition, data->endPosition, t);
        object->setPosition(newPosition);
        };
    animations.emplace_back(moveFunc, duration, data);
    return *this;
}

AnimationBuilder& AnimationBuilder::rotate(SceneObject* object, glm::vec3 startRotation, glm::vec3 endRotation, float duration) {
    auto data = std::make_shared<RotateAnimationData>();
    data->startRotation = startRotation;
    data->endRotation = endRotation;
    std::function<void(float)> rotateFunc = [object, data](float t) {
        glm::vec3 newRotation = glm::mix(data->startRotation, data->endRotation, t);
        object->setRotation(newRotation);
        };
    animations.emplace_back(rotateFunc, duration, data);
    return *this;
}

AnimationBuilder& AnimationBuilder::orbit(SceneObject* object, glm::vec3 center, float radius, float angularSpeed, float duration, glm::vec3 axis, float phaseShift) {
    auto data = std::make_shared<OrbitAnimationData>();
    data->center = center;
    data->radius = radius;
    data->angularSpeed = angularSpeed;
    data->axis = axis;
    data->phaseShift = phaseShift;
    std::function<void(float)> orbitFunc = [object, data, duration](float t) {
        float angle = glm::radians(data->angularSpeed) * duration * t + glm::radians(data->phaseShift);

        glm::vec3 newPosition;
        if (data->axis == glm::vec3(1.0f, 0.0f, 0.0f)) {
            newPosition.x = data->center.x;
            newPosition.y = data->center.y + data->radius * cos(angle);
            newPosition.z = data->center.z + data->radius * sin(angle);
        }
        else if (data->axis == glm::vec3(0.0f, 1.0f, 0.0f)) {
            newPosition.x = data->center.x + data->radius * cos(angle);
            newPosition.y = data->center.y;
            newPosition.z = data->center.z + data->radius * sin(angle);
        }
        else if (data->axis == glm::vec3(0.0f, 0.0f, 1.0f)) {
            newPosition.x = data->center.x + data->radius * cos(angle);
            newPosition.y = data->center.y + data->radius * sin(angle);
            newPosition.z = data->center.z;
        }
        else {
            newPosition.x = data->center.x + data->radius * cos(angle);
            newPosition.y = data->center.y;
            newPosition.z = data->center.z + data->radius * sin(angle);
        }

        object->setPosition(newPosition);
        };
    animations.emplace_back(orbitFunc, duration, data);
    return *this;
}

AnimationBuilder& AnimationBuilder::custom(std::function<void(float)> customFunc, float duration) {
    animations.emplace_back(customFunc, duration, nullptr);
    return *this;
}

AnimationBuilder& AnimationBuilder::wait(SceneObject* object, float duration) {
    auto data = std::make_shared<WaitAnimationData>();
    std::function<void(float)> waitFunc = [object, data, duration](float t) {
        //Do nothing
        };
    animations.emplace_back(waitFunc, duration, data);
    return *this;
}

std::shared_ptr<AnimationSequence> AnimationBuilder::build() {
    auto sequence = std::make_shared<AnimationSequence>();
    for (auto& anim : animations) {
        sequence->addAnimation(anim);
    }
    return sequence;
}

void AnimationBuilder::build(AnimationSequence& sequence)
{
    for (auto& anim : animations) {
        sequence.addAnimation(anim);
    }
}
