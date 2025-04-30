#include "AssetCollectionSystem.h"
#include "MaterialComponent.h"
#include "MeshComponent.h"
#include "AssetManager.h"
#include "Engine.h"

void AssetCollectionSystem::update(ContextType& context) {
    auto& registry = context.getRegistry();
    AssetManager& assetManager = Engine::get().assetManager();

    std::unordered_set<AssetHandle> assetsToLoad;

    // Zbieranie assetów z komponentów materia³ów
    auto materialEntities = registry.createView<MaterialComponent>();
    for (auto entity : materialEntities) {
        auto& material = registry.getComponent<MaterialComponent>(entity);
        assetsToLoad.insert(material.getMaterial());
    }

    // Zbieranie assetów z komponentów meshów
    auto meshEntities = registry.createView<MeshComponent>();
    for (auto entity : meshEntities) {
        auto& mesh = registry.getComponent<MeshComponent>(entity);
        assetsToLoad.insert(mesh.getMesh());
    }

    // £adowanie wszystkich zebranych assetów
    for (const auto& handle : assetsToLoad) {
        assetManager.ensureLoaded(handle);
    }
}