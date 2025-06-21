#include "MeshRenderSystem.h"
#include "TransformComponent.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "RenderSystem.h"
#include "RenderOrder.h"
#include "Engine.h"

void MeshRenderSystem::update() {
    auto meshes = m_registry->components().createView<TransformComponent, MeshComponent, MaterialComponent>();

    for (Entity entity : meshes) {
        auto renderOrder = std::make_shared<MeshRenderOrder>();
        renderOrder->entity = entity;

        m_engine->renderSystem().submitRenderOrder(renderOrder);
    }
}