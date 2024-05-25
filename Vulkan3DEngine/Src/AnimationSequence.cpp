#include "AnimationSequence.h"
#include "Animation.h"
#include "AnimationBuilder.h"

void AnimationSequence::addAnimation(Animation& animation) {
    m_animations.push_back(animation);
}

void AnimationSequence::update()
{
    if (m_animations.empty()) {
        //fmt::print("No animations in the sequence.\n");
        return;
    }

    m_animations[m_currentAnimation].update();
    if (!m_animations[m_currentAnimation].isActive()) {
        m_currentAnimation++;
        if (m_currentAnimation >= m_animations.size()) {
            if (m_loop) {
                m_currentAnimation = 0; // loop the sequence
                for (auto& anim : m_animations) {
                    anim.reset();
                }
            }
            else {
                m_currentAnimation = m_animations.size(); // end the sequence
            }
        }
    }
    //fmt::print("Animation sequence updated. Current animation: {}\n", m_currentAnimation);
}

void AnimationSequence::to_json(nlohmann::json& j) const {
    j["loop"] = m_loop;
    j["animations"] = nlohmann::json::array(); // Initialize as an empty array
    for (const auto& anim : m_animations) {
        nlohmann::json animJson;
        try {
            anim.to_json(animJson);
            j["animations"].push_back(animJson);
        }
        catch (const std::exception& e) {
            fmt::print(stderr, "Error: Failed to serialize animation. Exception: {}\n", e.what());
        }
    }
}

void AnimationSequence::from_json(const nlohmann::json& j, SceneObject* object) {
    try {
        m_loop = j.at("loop").get<bool>();

        AnimationBuilder builder;

        // Clear existing animations
        m_animations.clear();

        // Iterate over animations array in JSON
        for (const auto& animJson : j.at("animations")) {
            try {
                float duration = animJson.at("duration");

                if (animJson.contains("data")) {
                    auto dataJson = animJson.at("data");

                    if (dataJson["type"] == "move") {
                        glm::vec3 startPosition(dataJson["startPosition"][0], dataJson["startPosition"][1], dataJson["startPosition"][2]);
                        glm::vec3 endPosition(dataJson["endPosition"][0], dataJson["endPosition"][1], dataJson["endPosition"][2]);

                        builder.move(object, startPosition, endPosition, duration);
                    }
                    else if (dataJson["type"] == "rotate") {
                        glm::vec3 startRotation(dataJson["startRotation"][0], dataJson["startRotation"][1], dataJson["startRotation"][2]);
                        glm::vec3 endRotation(dataJson["endRotation"][0], dataJson["endRotation"][1], dataJson["endRotation"][2]);

                        builder.rotate(object, startRotation, endRotation, duration);
                    }
                    else if (dataJson["type"] == "orbit") {
                        glm::vec3 center(dataJson["center"][0], dataJson["center"][1], dataJson["center"][2]);
                        float radius = dataJson["radius"];
                        float angularSpeed = dataJson["angularSpeed"];
                        glm::vec3 axis(dataJson["axis"][0], dataJson["axis"][1], dataJson["axis"][2]);
                        float phaseShift = dataJson.contains("phaseShift") ? dataJson["phaseShift"].get<float>() : 0.0f;

                        builder.orbit(object, center, radius, angularSpeed, duration, axis, phaseShift);
                    }
                    else if (dataJson["type"] == "wait") {

                        builder.wait(object, duration);
                    }
                }
                else {
                    builder.custom([](float) {}, duration);
                }
            }
            catch (const std::exception& e) {
                fmt::print(stderr, "Error: Failed to deserialize animation. Exception: {}\n", e.what());
                throw;
            }
        }
        builder.build(*this);

        fmt::print("AnimationSequence initialized successfully\n");
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Error: Failed to read 'loop' from JSON. Exception: {}\n", e.what());
        throw;
    }
}




