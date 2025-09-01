#include "LightSystem.h"
#include "TransformComponent.h"
#include "LightComponent.h"
#include "RenderSystem.h"
#include "RenderOrder.h"
#include "Engine.h"
#include "LightRenderOrder.h"

void LightSystem::update() {
    auto lights = m_registry->components().createView<LightComponent, TransformComponent>();

    for (Entity entity : lights) {
        auto renderOrder = std::make_shared<LightRenderOrder>();
        renderOrder->entity = entity;

        m_engine->renderSystem().submitRenderOrder(renderOrder);
    }
}