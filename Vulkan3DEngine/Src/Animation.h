#pragma once
#include "Prerequisites.h"
#include <functional>
#include <chrono>
#include <cmath>
#include <atomic>
#include <condition_variable>
#include <mutex>

class Animation {
public:
    Animation(std::function<void(float)> updateFunc, float duration)
        : m_updateFunc(updateFunc), m_duration(duration), m_active(true) {}

    // Explicitly delete the copy constructor and copy assignment operator
    //Animation(const Animation&) = delete;
    //Animation& operator=(const Animation&) = delete;

    // Define the move constructor and move assignment operator
    //Animation(Animation&& other) noexcept;
    //Animation& operator=(Animation&& other) noexcept;

    void update();

    bool isActive() const { return m_active; }
    void reset() {
        m_active = true;
        m_elapsedTime = 0.0f;
    }

private:
    std::function<void(float)> m_updateFunc;
    float m_duration;
    float m_elapsedTime = 0.0f;
    bool m_active;

    friend class SceneObjectManager;
    friend class AnimationBuilder;
};

