#include "Animation.h"
#include "Application.h"

//// Implementation of the move constructor
//Animation::Animation(Animation&& other) noexcept
//    : m_updateFunc(std::move(other.m_updateFunc)),
//    m_duration(other.m_duration),
//    m_elapsedTime(other.m_elapsedTime),
//    m_active(other.m_active) {}
//
//// Implementation of the move assignment operator
//Animation& Animation::operator=(Animation&& other) noexcept {
//    if (this != &other) {
//        m_updateFunc = std::move(other.m_updateFunc);
//        m_duration = other.m_duration;
//        m_elapsedTime = other.m_elapsedTime;
//        m_active = other.m_active;
//    }
//    return *this;
//}

void Animation::update()
{
    if (!m_active) {
        //fmt::print("Animation is not active.\n");
        return;
    }

    m_elapsedTime += Application::s_deltaTime;
    if (m_elapsedTime > m_duration) {
        m_active = false;
        m_elapsedTime = m_duration;
    }

    float t = m_elapsedTime / m_duration;
    m_updateFunc(t);
    //fmt::print("Animation updated. Elapsed time: {}, t: {}\n", m_elapsedTime, t);
}