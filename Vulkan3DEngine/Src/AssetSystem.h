// AssetSystem.h
#pragma once
#include "AssetManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "MaterialManager.h"
#include "ShaderManager.h"
#include "PrefabManager.h"
#include "VramManager.h"
#include "LogicalDevice.h"
#include "Registry.h"
#include <memory>

class Renderer; // Forward declaration

class AssetSystem {
public:
    AssetSystem(Renderer& renderer);
    ~AssetSystem();

    // Core asset system methods
    void advanceFrame();

    // Access to managers
    AssetManager& assetManager() { return *m_assetManager; }
    TextureManager& textureManager() { return *m_textureManager; }
    MeshManager& meshManager() { return *m_meshManager; }
    MaterialManager& materialManager() { return *m_materialManager; }
    ShaderManager& shaderManager() { return *m_shaderManager; }
    PrefabManager& prefabManager() { return *m_prefabManager; }
private:
    Renderer& m_renderer;
    std::unique_ptr<AssetManager> m_assetManager;

    // Asset handlers
    std::shared_ptr<TextureManager> m_textureManager;
    std::shared_ptr<MeshManager> m_meshManager;
    std::shared_ptr<MaterialManager> m_materialManager;
    std::shared_ptr<ShaderManager> m_shaderManager;
    std::shared_ptr<PrefabManager> m_prefabManager;

    // Register all handlers with the asset manager
    void registerAssetHandlers();
};