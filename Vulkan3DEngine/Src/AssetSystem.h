#pragma once
#include "AssetManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "MaterialManager.h"
#include "ShaderManager.h"
#include "PrefabManager.h"
#include "SceneManager.h"
#include "VramManager.h"
#include "LogicalDevice.h"
#include "ComponentManager.h"
#include "RenderGraphTemplateManager.h"
#include <memory>

class EngineCore; // Forward declaration

class AssetSystem {
public:
    AssetSystem(EngineCore& renderer, ThreadPool& threadPool);
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
    SceneManager& sceneManager() { return *m_sceneManager; }
    RenderGraphTemplateManager& renderGraphTemplateManager() { return *m_renderGraphTemplateManager; }
private:
    EngineCore& m_renderer;
    std::unique_ptr<AssetManager> m_assetManager;

    // Asset handlers
    std::shared_ptr<TextureManager> m_textureManager;
    std::shared_ptr<MeshManager> m_meshManager;
    std::shared_ptr<MaterialManager> m_materialManager;
    std::shared_ptr<ShaderManager> m_shaderManager;
    std::shared_ptr<PrefabManager> m_prefabManager;
    std::shared_ptr<SceneManager> m_sceneManager;
    std::shared_ptr<RenderGraphTemplateManager> m_renderGraphTemplateManager;

    // Register all handlers with the asset manager
    void registerAssetHandlers();
};
