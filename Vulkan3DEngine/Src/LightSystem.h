#pragma once
#include "System.h"
#include "TransformComponent.h"
#include "LightComponent.h"
#include "RenderSystem.h"
#include "RenderOrder.h"
#include "Engine.h"

class LightSystem : public System<> {
public:
    void update(ContextType& context) override;
private:

};