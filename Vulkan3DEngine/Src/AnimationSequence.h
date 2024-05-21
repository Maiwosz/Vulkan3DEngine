#pragma once
#include "Prerequisites.h"
#include "Animation.h"
#include <vector>

class AnimationSequence {
public:
    AnimationSequence(bool loop = true)
        : m_loop(loop) {}

    void addAnimation(Animation& animation);

    void update();

    void setLoop(bool loop) { m_loop = loop; }

private:
    std::vector<Animation> m_animations;
    size_t m_currentAnimation = 0;
    bool m_loop;

    friend class SceneObjectManager;
};