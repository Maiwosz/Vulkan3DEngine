#include "CameraSystem.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "RenderSystem.h"
#include "RenderOrder.h"
#include "Engine.h"

void CameraSystem::update() {
    auto cameras = m_registry->components().createView<CameraComponent, TransformComponent>();

    for (Entity entity : cameras) {
        auto renderOrder = std::make_shared<CameraRenderOrder>();
        renderOrder->entity = entity;

        m_engine->renderSystem().submitRenderOrder(renderOrder);
    }
}