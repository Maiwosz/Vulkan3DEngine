#pragma once
#include "System.h"

class CameraSystem : public System<> {
public:
    void update() override;
};