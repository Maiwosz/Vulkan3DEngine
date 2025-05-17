#include "LightSystem.h"


void LightSystem::update(ContextType& context) {
    Registry& registry = context.getRegistry();
    auto lights = registry.createView<LightComponent, TransformComponent>();

    for (Entity entity : lights) {
        auto renderOrder = std::make_shared<LightRenderOrder>();
        renderOrder->entity = entity;

        Engine::get().renderSystem().submitRenderOrder(renderOrder);
    }
}