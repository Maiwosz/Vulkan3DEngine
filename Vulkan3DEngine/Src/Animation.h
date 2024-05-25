#pragma once
#include "Prerequisites.h"
#include <functional>
#include <chrono>
#include <cmath>
#include <atomic>
#include <condition_variable>
#include <mutex>

class AnimationData {
public:
    virtual ~AnimationData() = default;
    virtual void to_json(nlohmann::json& j) const = 0;
    virtual void from_json(const nlohmann::json& j) = 0;
};

class Animation {
public:
    Animation(std::function<void(float)> updateFunc, float duration, std::shared_ptr<AnimationData> data = nullptr)
        : m_updateFunc(updateFunc), m_duration(duration), m_active(true), m_data(data) {}

    void update();

    bool isActive() const { return m_active; }
    void reset() {
        m_active = true;
        m_elapsedTime = 0.0f;
    }

    std::shared_ptr<AnimationData> getData() const { return m_data; }

    void to_json(nlohmann::json& j) const;
    void from_json(const nlohmann::json& j);

private:
    std::function<void(float)> m_updateFunc;
    float m_duration;
    float m_elapsedTime = 0.0f;
    bool m_active;
    std::shared_ptr<AnimationData> m_data;

    friend class SceneObjectManager;
    friend class AnimationBuilder;
    friend class AnimationSequence;
};
