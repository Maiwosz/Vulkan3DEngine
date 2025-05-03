#include "MeshRenderSystem.h"
#include "TransformComponent.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "RenderSystem.h"
#include "RenderOrder.h"
#include "Engine.h"

void MeshRenderSystem::update(ContextType& context) {
    Registry& registry = context.getRegistry();
    auto meshes = registry.createView<TransformComponent, MeshComponent, MaterialComponent>();

    for (Entity entity : meshes) {
        auto renderOrder = std::make_shared<MeshRenderOrder>();
        renderOrder->entity = entity;

        Engine::get().renderSystem().submitRenderOrder(renderOrder);
    }
}