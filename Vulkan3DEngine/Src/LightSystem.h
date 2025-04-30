#pragma once
#include "System.h"
#include "Registry.h"
#include "TransformComponent.h"
#include "PointLightComponent.h"
#include "DirectionalLightComponent.h"

class LightSystem : public System<> {
public:

    void update(ContextType& context) override;


private:

};