#pragma once
#include "System.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class CameraSystem : public System<> {
public:
    void update(ContextType& context) override;

private:

};