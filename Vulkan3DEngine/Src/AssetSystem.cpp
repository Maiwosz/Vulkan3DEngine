// AssetSystem.cpp
#include "AssetSystem.h"
#include "Renderer.h"
#include "Engine.h"
#include <spdlog/spdlog.h>

AssetSystem::AssetSystem(Renderer& renderer)
    : m_renderer(renderer)
{
    SPDLOG_DEBUG("Creating AssetSystem");

    // Create the asset manager
    m_assetManager = std::make_unique<AssetManager>(
        m_renderer.vramManager()
    );

    // Create asset handlers
    m_shaderManager = std::make_shared<ShaderManager>(
        m_renderer.vulkanContext().logical(),
        m_renderer.shaderModuleManager(),
        m_renderer.descriptorLayoutManager(),
        m_renderer.pipelineLayoutManager()
    );

    m_textureManager = std::make_shared<TextureManager>(
        m_renderer.vulkanContext().logical(),
        m_renderer.vramManager()
    );

    m_materialManager = std::make_shared<MaterialManager>(
        m_renderer.vulkanContext().logical(),
        *m_shaderManager,
        m_renderer.imageSamplerManager(),
        m_renderer.uniformBufferManager(),
        m_renderer.descriptorAllocator(),
        m_renderer.descriptorLayoutManager(),
        *m_textureManager
    );

    m_meshManager = std::make_shared<MeshManager>(
        m_renderer.vramManager()
    );

    m_prefabManager = std::make_shared<PrefabManager>();

    // Register all handlers with the asset manager
    registerAssetHandlers();

    SPDLOG_INFO("AssetSystem initialization complete");
}

AssetSystem::~AssetSystem()
{
    SPDLOG_DEBUG("Destroying AssetSystem");
    // Close in reverse order of creation
    m_materialManager.reset();
    m_textureManager.reset();
    m_meshManager.reset();
    m_shaderManager.reset();
    m_prefabManager.reset();
    m_assetManager.reset();
}

void AssetSystem::advanceFrame()
{
    if (m_assetManager) {
        m_assetManager->advanceFrame();
    }
}

void AssetSystem::registerAssetHandlers()
{
    SPDLOG_DEBUG("Registering asset handlers");

    m_assetManager->registerHandler(AssetType::Mesh, m_meshManager);
    m_assetManager->registerHandler(AssetType::Texture, m_textureManager);
    m_assetManager->registerHandler(AssetType::Shader, m_shaderManager);
    m_assetManager->registerHandler(AssetType::Material, m_materialManager);
    m_assetManager->registerHandler(AssetType::Prefab, m_prefabManager);
}