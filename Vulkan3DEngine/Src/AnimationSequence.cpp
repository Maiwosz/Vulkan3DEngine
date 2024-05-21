#include "AnimationSequence.h"
#include "Animation.h"

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
