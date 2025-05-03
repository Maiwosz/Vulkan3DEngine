#include "CameraSystem.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "RenderSystem.h"
#include "RenderOrder.h"
#include "Engine.h"

void CameraSystem::update(ContextType& context) {
    Registry& registry = context.getRegistry();
    auto cameras = registry.createView<CameraComponent, TransformComponent>();

    for (Entity entity : cameras) {
        auto renderOrder = std::make_shared<CameraRenderOrder>();
        renderOrder->entity = entity;

        Engine::get().renderSystem().submitRenderOrder(renderOrder);
    }
}