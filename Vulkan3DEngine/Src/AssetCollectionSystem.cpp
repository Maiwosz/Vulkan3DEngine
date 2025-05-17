#include "AssetCollectionSystem.h"

void AssetCollectionSystem::update(ContextType& context) {
    auto& registry = context.getRegistry();
    AssetManager& assetManager = Engine::get().assetManager();

    std::unordered_set<AssetHandle> assetsToLoad;

    // Zbieranie assetów z komponentów materiałów
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

    // Ładowanie wszystkich zebranych assetów
    for (const auto& handle : assetsToLoad) {
        assetManager.ensureLoaded(handle);
    }
}