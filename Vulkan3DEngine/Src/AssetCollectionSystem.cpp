#include "AssetCollectionSystem.h"

void AssetCollectionSystem::update() {
    AssetManager& assetManager = m_engine->assetSystem().assetManager();

    std::unordered_set<AssetHandle> assetsToLoad;

    // Zbieranie assetów z komponentów materiałów
    auto materialEntities = m_registry->components().createView<MaterialComponent>();
    for (auto entity : materialEntities) {
        auto& material = m_registry->components().getComponent<MaterialComponent>(entity);
        assetsToLoad.insert(material.getMaterial());
    }

    // Zbieranie assetów z komponentów meshów
    auto meshEntities = m_registry->components().createView<MeshComponent>();
    for (auto entity : meshEntities) {
        auto& mesh = m_registry->components().getComponent<MeshComponent>(entity);
        assetsToLoad.insert(mesh.getMesh());
    }

    // Ładowanie wszystkich zebranych assetów
    for (const auto& handle : assetsToLoad) {
        assetManager.ensureLoaded(handle);
    }
}